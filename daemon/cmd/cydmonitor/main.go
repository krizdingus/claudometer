package main

import (
	"context"
	"fmt"
	"io/fs"
	"os"
	"os/signal"
	"path/filepath"
	"sync"
	"syscall"
	"time"

	"github.com/krizdingus/cydmonitor/daemon/internal/claudedata"
	"github.com/krizdingus/cydmonitor/daemon/internal/cli"
	"github.com/krizdingus/cydmonitor/daemon/internal/discovery"
	"github.com/krizdingus/cydmonitor/daemon/internal/pairings"
	"github.com/krizdingus/cydmonitor/daemon/internal/routines"
	"github.com/krizdingus/cydmonitor/daemon/internal/server"
	"github.com/krizdingus/cydmonitor/daemon/internal/stats"
)

const (
	version    = "0.1.0-dev"
	listenAddr = "0.0.0.0:7842"
	statusURL  = "http://127.0.0.1:7842"
)

func main() {
	if len(os.Args) > 1 {
		switch os.Args[1] {
		case "version":
			fmt.Println(version)
			return
		case "status":
			os.Exit(cli.Status(os.Stdout, statusURL))
		case "reset-pairings":
			os.Exit(cli.ResetPairings(os.Stdout, pairingsPath()))
		}
	}
	if err := serve(); err != nil {
		fmt.Fprintln(os.Stderr, "fatal:", err)
		os.Exit(1)
	}
}

func serve() error {
	home, err := os.UserHomeDir()
	if err != nil {
		return err
	}

	store, err := pairings.NewStore(pairingsPath())
	if err != nil {
		return fmt.Errorf("pairings: %w", err)
	}
	codes := pairings.NewCodes(2 * time.Minute)

	// Load Claude data
	claudeDir := filepath.Join(home, ".claude")
	planInfo, _ := claudedata.ReadPlanInfo(filepath.Join(claudeDir, ".claude.json"))
	projectsDir := filepath.Join(claudeDir, "projects")

	initial, err := loadAllJSONL(projectsDir)
	if err != nil {
		fmt.Fprintf(os.Stderr, "warning: could not scan Claude projects: %v\n", err)
	}
	cache := &recordsCache{}
	cache.set(initial)

	agg := &stats.Aggregator{
		GetRecords: cache.get,
		PlanInfo:   planInfo,
		Routines:   routines.NewFetcher(60 * time.Second),
		Now:        time.Now,
	}

	srv := server.New(server.Config{
		Store: store, Codes: codes, Aggregator: agg, Version: version,
	})

	ctx, cancel := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer cancel()

	hostname, _ := os.Hostname()
	var wg sync.WaitGroup
	wg.Add(3)
	go func() { defer wg.Done(); _ = discovery.Advertise(ctx, "Claude Monitor", hostname, 7842, version, stats.SchemaVersion) }()
	go func() { defer wg.Done(); _ = srv.ListenAndServe(ctx, listenAddr) }()
	go func() { defer wg.Done(); reloadLoop(ctx, projectsDir, cache, 30*time.Second) }()
	wg.Wait()
	return nil
}

// recordsCache holds the most recent snapshot of parsed Claude records. The
// daemon refreshes it from disk on a timer so newly-written assistant turns
// show up in /v1/stats responses without restarting cydmonitor.
type recordsCache struct {
	mu   sync.RWMutex
	recs []claudedata.Record
}

func (c *recordsCache) get() []claudedata.Record {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.recs
}

func (c *recordsCache) set(r []claudedata.Record) {
	c.mu.Lock()
	c.recs = r
	c.mu.Unlock()
}

func reloadLoop(ctx context.Context, projectsDir string, cache *recordsCache, interval time.Duration) {
	t := time.NewTicker(interval)
	defer t.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case <-t.C:
			recs, err := loadAllJSONL(projectsDir)
			if err != nil {
				fmt.Fprintf(os.Stderr, "warning: reload scan failed: %v\n", err)
				continue
			}
			cache.set(recs)
		}
	}
}

func loadAllJSONL(projectsDir string) ([]claudedata.Record, error) {
	var out []claudedata.Record
	err := filepath.WalkDir(projectsDir, func(path string, d fs.DirEntry, walkErr error) error {
		if walkErr != nil {
			fmt.Fprintf(os.Stderr, "warning: walk %s: %v\n", path, walkErr)
			return nil
		}
		if d.IsDir() || filepath.Ext(d.Name()) != ".jsonl" {
			return nil
		}
		recs, err := claudedata.ParseFile(path)
		if err != nil {
			fmt.Fprintf(os.Stderr, "warning: skipping %s: %v\n", path, err)
			return nil
		}
		out = append(out, recs...)
		return nil
	})
	if err != nil {
		return nil, err
	}
	return claudedata.Dedup(out), nil
}

func pairingsPath() string {
	home, _ := os.UserHomeDir()
	return filepath.Join(home, ".config", "cydmonitor", "pairings.json")
}
