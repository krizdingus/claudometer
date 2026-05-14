// Package runner assembles the cydmonitor service stack (pairings, records
// cache, reload loop, aggregator, HTTP server, mDNS advertiser) from a single
// Options struct. The terminal binary and the desktop app both build through
// it so they share lifecycle and behavior.
package runner

import (
	"context"
	"fmt"
	"io/fs"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"sync"
	"time"

	"github.com/krizdingus/cydmonitor/daemon/pkg/claudedata"
	"github.com/krizdingus/cydmonitor/daemon/pkg/discovery"
	"github.com/krizdingus/cydmonitor/daemon/pkg/pairings"
	"github.com/krizdingus/cydmonitor/daemon/pkg/routines"
	"github.com/krizdingus/cydmonitor/daemon/pkg/server"
	"github.com/krizdingus/cydmonitor/daemon/pkg/stats"
)

type Options struct {
	ListenAddr     string
	ProjectsDir    string
	PairingsPath   string
	PlanInfo       claudedata.PlanInfo
	Caps           claudedata.Caps
	ReloadInterval time.Duration
	AdvertiseName  string
	Version        string
	Logger         func(format string, args ...any)
}

type Service struct {
	opts  Options
	addr  string
	addrM sync.RWMutex

	cache *recordsCache
	store *pairings.Store
	codes *pairings.Codes
	agg   *stats.Aggregator

	healthM sync.RWMutex
	health  Health
}

type Health struct {
	ListenerUp   bool
	PairedCount  int
	LastReloadOK bool
	LastReloadAt time.Time
	RecordsCount int
}

func New(opts Options) (*Service, error) {
	if opts.ReloadInterval <= 0 {
		opts.ReloadInterval = 30 * time.Second
	}
	if opts.AdvertiseName == "" {
		opts.AdvertiseName = "Claude Monitor"
	}
	if opts.Logger == nil {
		opts.Logger = func(f string, a ...any) { fmt.Fprintf(os.Stderr, f+"\n", a...) }
	}
	store, err := pairings.NewStore(opts.PairingsPath)
	if err != nil {
		return nil, fmt.Errorf("pairings: %w", err)
	}
	cache := &recordsCache{}
	agg := &stats.Aggregator{
		GetRecords: cache.get,
		PlanInfo:   opts.PlanInfo,
		Caps:       opts.Caps,
		Routines:   routines.NewFetcher(60 * time.Second),
		Now:        time.Now,
	}
	return &Service{
		opts:  opts,
		cache: cache,
		store: store,
		codes: pairings.NewCodes(2 * time.Minute),
		agg:   agg,
	}, nil
}

func (s *Service) Addr() string {
	s.addrM.RLock()
	defer s.addrM.RUnlock()
	return s.addr
}

func (s *Service) Health() Health {
	s.healthM.RLock()
	defer s.healthM.RUnlock()
	h := s.health
	h.PairedCount = len(s.store.List())
	return h
}

func (s *Service) Start(ctx context.Context) error {
	ln, err := net.Listen("tcp", s.opts.ListenAddr)
	if err != nil {
		return fmt.Errorf("listen %s: %w", s.opts.ListenAddr, err)
	}
	s.setAddr(ln.Addr().String())
	s.markListenerUp(true)
	defer s.markListenerUp(false)

	if recs, err := s.loadAllJSONL(); err != nil {
		s.opts.Logger("warning: initial scan failed: %v", err)
		s.recordReload(false, 0)
	} else {
		s.cache.set(recs)
		s.recordReload(true, len(recs))
	}

	srv := server.New(server.Config{
		Store: s.store, Codes: s.codes, Aggregator: s.agg, Version: s.opts.Version,
	})
	httpSrv := &http.Server{Handler: srv.Handler()}
	hostname, _ := os.Hostname()

	var wg sync.WaitGroup
	wg.Add(3)
	go func() {
		defer wg.Done()
		_ = discovery.Advertise(ctx, s.opts.AdvertiseName, hostname,
			portFromAddr(ln.Addr().String()), s.opts.Version, stats.SchemaVersion)
	}()
	go func() {
		defer wg.Done()
		<-ctx.Done()
		_ = httpSrv.Close()
	}()
	go func() {
		defer wg.Done()
		s.reloadLoop(ctx)
	}()

	if err := httpSrv.Serve(ln); err != nil && err != http.ErrServerClosed {
		s.opts.Logger("http: %v", err)
	}
	wg.Wait()
	return nil
}

func (s *Service) setAddr(a string) {
	s.addrM.Lock()
	s.addr = a
	s.addrM.Unlock()
}

func (s *Service) markListenerUp(up bool) {
	s.healthM.Lock()
	s.health.ListenerUp = up
	s.healthM.Unlock()
}

func (s *Service) recordReload(ok bool, count int) {
	s.healthM.Lock()
	s.health.LastReloadOK = ok
	s.health.LastReloadAt = time.Now()
	if ok {
		s.health.RecordsCount = count
	}
	s.healthM.Unlock()
}

func (s *Service) reloadLoop(ctx context.Context) {
	t := time.NewTicker(s.opts.ReloadInterval)
	defer t.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case <-t.C:
			recs, err := s.loadAllJSONL()
			if err != nil {
				s.opts.Logger("warning: reload failed: %v", err)
				s.recordReload(false, 0)
				continue
			}
			s.cache.set(recs)
			s.recordReload(true, len(recs))
		}
	}
}

func (s *Service) loadAllJSONL() ([]claudedata.Record, error) {
	var out []claudedata.Record
	err := filepath.WalkDir(s.opts.ProjectsDir, func(path string, d fs.DirEntry, walkErr error) error {
		if walkErr != nil {
			s.opts.Logger("warning: walk %s: %v", path, walkErr)
			return nil
		}
		if d.IsDir() || filepath.Ext(d.Name()) != ".jsonl" {
			return nil
		}
		recs, err := claudedata.ParseFile(path)
		if err != nil {
			s.opts.Logger("warning: skipping %s: %v", path, err)
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

func portFromAddr(addr string) int {
	_, p, err := net.SplitHostPort(addr)
	if err != nil {
		return 0
	}
	var port int
	fmt.Sscanf(p, "%d", &port)
	return port
}

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
