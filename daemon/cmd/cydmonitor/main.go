package main

import (
	"context"
	"fmt"
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
	records, err := loadAllJSONL(filepath.Join(claudeDir, "projects"))
	if err != nil {
		fmt.Fprintf(os.Stderr, "warning: could not scan Claude projects: %v\n", err)
	}

	agg := &stats.Aggregator{
		Records:  records,
		PlanInfo: planInfo,
		Routines: routines.NewFetcher(60 * time.Second),
		Now:      time.Now,
	}

	srv := server.New(server.Config{
		Store: store, Codes: codes, Aggregator: agg, Version: version,
	})

	ctx, cancel := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer cancel()

	hostname, _ := os.Hostname()
	var wg sync.WaitGroup
	wg.Add(2)
	go func() { defer wg.Done(); _ = discovery.Advertise(ctx, "Claude Monitor", hostname, 7842, version, stats.SchemaVersion) }()
	go func() { defer wg.Done(); _ = srv.ListenAndServe(ctx, listenAddr) }()
	wg.Wait()
	return nil
}

func loadAllJSONL(projectsDir string) ([]claudedata.Record, error) {
	entries, err := os.ReadDir(projectsDir)
	if err != nil {
		return nil, err
	}
	var out []claudedata.Record
	for _, e := range entries {
		if e.IsDir() || filepath.Ext(e.Name()) != ".jsonl" {
			continue
		}
		recs, err := claudedata.ParseFile(filepath.Join(projectsDir, e.Name()))
		if err != nil {
			fmt.Fprintf(os.Stderr, "warning: skipping %s: %v\n", e.Name(), err)
			continue
		}
		out = append(out, recs...)
	}
	return out, nil
}

func pairingsPath() string {
	home, _ := os.UserHomeDir()
	return filepath.Join(home, ".config", "cydmonitor", "pairings.json")
}
