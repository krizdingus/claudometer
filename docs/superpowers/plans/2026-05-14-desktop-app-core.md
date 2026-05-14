# Desktop App: Core + Daemon Embedding Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a runnable macOS Wails desktop application (`CYDMonitor.app`) that embeds the existing Go daemon services on `:7842`, shows a system tray with a status indicator, reads runtime caps + screen toggles from a JSON settings file, opens a placeholder webview window, and supports launch-at-login. The existing `cydmonitor` terminal binary keeps building and working for Linux/dev users.

**Architecture:** The current `daemon/internal/*` packages get promoted to `daemon/pkg/*` so a sibling `desktop/` Go module can import them. A new `daemon/pkg/runner` package extracts the service-assembly logic (pairings + records cache + aggregator + server + mDNS + reload loop) out of `daemon/cmd/cydmonitor/main.go` so both the terminal binary and the Wails app build the same supervised stack from one `Options` struct. The Wails app adds three new pieces: a `settings` package that loads `~/Library/Application Support/CYDMonitor/settings.json` and produces a `claudedata.Caps` override (file > env vars > plan defaults), a tray icon driven by `getlantern/systray` running in the Wails OS-thread, and a launch-at-login toggle using a LaunchAgent plist on macOS.

**Tech Stack:** Wails v2.12.0 (Go 1.23 + WKWebView), getlantern/systray for the tray, plain HTML/CSS/JS for the placeholder webview (no JS framework yet — defer that to Plan C when the real settings UI lands), Go workspaces (`go.work`) for the multi-module layout. macOS-only in this plan; Windows is explicitly out of scope and gets added in Plan C.

---

## Spec section covered

This plan implements the **Architecture**, **Components → Reused / Removed / New (tray, launch-at-login, settings store, webview UI skeleton)** sections of `docs/superpowers/specs/2026-05-13-desktop-app-design.md`. It does **not** implement Flasher, USB Provisioner, Add Device wizard, full settings UI pages, auto-update, or distribution/signing — those are Plans B and C.

## What success looks like

After this plan:

1. `cd desktop && wails build` produces `desktop/build/bin/CYDMonitor.app` that launches without errors on macOS 14+.
2. The app opens a 480×320 webview window showing a placeholder page that says "CYDMonitor is running — daemon listening on http://localhost:7842". Closing the window hides it; only "Quit" from the tray exits.
3. The tray icon appears in the menu bar with menu items: app name + version (disabled), status indicator (Green/Yellow/Red), "Open settings…", "Add a device…" (disabled), "Quit".
4. `curl -H "Authorization: Bearer <token>" http://localhost:7842/v1/stats` returns the same schema-v1 JSON the terminal daemon returns. The token comes from `~/.config/cydmonitor/pairings.json` (shared with the terminal daemon).
5. The settings file at `~/Library/Application Support/CYDMonitor/settings.json` is created on first launch with sensible defaults; editing `plan_tier` in it (and relaunching) changes `/v1/stats` caps accordingly. CYDMONITOR_* env vars still work as a fallback when the file is absent.
6. Toggling "Launch at login" on writes `~/Library/LaunchAgents/com.krizdingus.cydmonitor.plist`; toggling off removes it.
7. Tray status goes green when the server is bound and the records cache has parsed at least one JSONL successfully; yellow when no devices are paired or JSONL parsing is stale; red when the listener failed to bind or the records cache is empty after the first reload cycle.
8. `make test` from `daemon/` still passes (zero changes to existing behavior). The new `desktop/` module has its own `go test ./...` that passes.
9. `cd daemon && go build ./cmd/cydmonitor && ./bin/cydmonitor` still works and behaves identically to today.

The Add Device button is intentionally disabled. The webview UI is intentionally a single placeholder page. Both come alive in Plans B and C.

## File structure

**Created:**
- `go.work` — workspace file at repo root linking `./daemon` and `./desktop`.
- `daemon/pkg/runner/runner.go` — `Options`, `Service`, `Service.Start(ctx) error` (blocks until ctx is cancelled). Owns the records cache, reload loop, aggregator, server, mDNS advertiser. No env-var reads, no signal handling. Just builds from `Options`.
- `daemon/pkg/runner/runner_test.go` — table tests for the records cache, the reload loop's swap behavior, and `Start` lifecycle (ctx cancellation shuts everything down).
- `desktop/go.mod` — new module `github.com/krizdingus/cydmonitor/desktop`, depends on the daemon module.
- `desktop/go.sum`
- `desktop/wails.json` — Wails project config (name CYDMonitor, frontend dir `frontend`, build options).
- `desktop/main.go` — Wails app entrypoint. Builds runner.Options from settings, starts the service in a goroutine, launches the webview + tray.
- `desktop/app.go` — Wails-bound application struct with methods callable from JS (`GetStatus`, `OpenSettings`, `Quit`). Tiny in this plan; grows in Plans B and C.
- `desktop/internal/settings/settings.go` — `Settings` struct, `Load(path)`, `Save(path)`, `(s Settings) Caps(plan claudedata.Plan) claudedata.Caps`.
- `desktop/internal/settings/settings_test.go` — round-trip tests + override precedence tests.
- `desktop/internal/tray/tray.go` — `Run(ctx, controller)` blocks on `systray.Run`. `controller` is an interface so we can test the menu-action callbacks without poking AppKit.
- `desktop/internal/tray/tray_test.go` — unit tests on the controller logic (status color computation, action dispatch).
- `desktop/internal/status/status.go` — `Watcher` goroutine that polls runner health and produces a `Color` (Green/Yellow/Red) the tray subscribes to.
- `desktop/internal/status/status_test.go`
- `desktop/internal/loginitem/loginitem_darwin.go` — Enable/Disable launch-at-login by writing/removing a LaunchAgent plist.
- `desktop/internal/loginitem/loginitem_darwin_test.go`
- `desktop/internal/loginitem/loginitem_other.go` — `//go:build !darwin` stub returning `ErrUnsupported` so the desktop module still builds on Linux for CI.
- `desktop/frontend/index.html` — placeholder page.
- `desktop/frontend/style.css`
- `desktop/frontend/main.js` — calls `window.go.main.App.GetStatus()` and renders the result.
- `desktop/frontend/package.json` — minimal scaffolding so `wails build` doesn't choke (no npm dependencies required for vanilla JS).
- `desktop/build/appicon.png` — placeholder icon (a 1024×1024 PNG, simple solid color is fine for this plan).
- `desktop/Taskfile.yml` (optional, omit if Makefile preferred).

**Modified:**
- `daemon/internal/*` → renamed to `daemon/pkg/*` (six packages: `claudedata`, `stats`, `server`, `pairings`, `routines`, `discovery`, `cli`). All import paths within the daemon module update.
- `daemon/cmd/cydmonitor/main.go` — replace the inline service-assembly logic with a call to `runner.New(opts).Start(ctx)`. Keep env-var reading and signal handling here (that's the terminal binary's job).
- `daemon/cmd/cydmonitor/main_e2e_test.go` — adjust any internal-package imports to `pkg/` paths.
- `daemon/Makefile` — add `desktop-build` and `desktop-dev` convenience targets that shell into `desktop/`.

**Deleted:**
- Nothing in this plan.

---

## Pre-flight

- [ ] **Step 0.1: Confirm Wails CLI is available**

Run:
```
wails version
```

Expected: prints `v2.12.0` (or `v2.12.x`). If absent: `go install github.com/wailsapp/wails/v2/cmd/wails@v2.12.0`, then re-run.

- [ ] **Step 0.2: Confirm daemon tests are currently green**

Run:
```
cd daemon && go test ./...
```

Expected: every package passes. If anything is red on this branch already, stop and tell the user — this plan assumes a clean baseline.

---

## Task 1: Promote daemon internal packages to pkg

The new `desktop/` module needs to import the daemon's `claudedata`, `stats`, `server`, `pairings`, `routines`, `discovery`, and `cli` packages. Go's `internal/` visibility rules forbid this across module boundaries, so we relocate them. No code changes, only paths.

**Files:**
- Modify: rename `daemon/internal/claudedata` → `daemon/pkg/claudedata`
- Modify: rename `daemon/internal/stats` → `daemon/pkg/stats`
- Modify: rename `daemon/internal/server` → `daemon/pkg/server`
- Modify: rename `daemon/internal/pairings` → `daemon/pkg/pairings`
- Modify: rename `daemon/internal/routines` → `daemon/pkg/routines`
- Modify: rename `daemon/internal/discovery` → `daemon/pkg/discovery`
- Modify: rename `daemon/internal/cli` → `daemon/pkg/cli`
- Modify: every `.go` file in `daemon/` that imports `github.com/krizdingus/cydmonitor/daemon/internal/*` — rewrite to `github.com/krizdingus/cydmonitor/daemon/pkg/*`.

- [ ] **Step 1.1: Move the directories**

Run from repo root:
```
mkdir -p daemon/pkg
git mv daemon/internal/claudedata daemon/pkg/claudedata
git mv daemon/internal/stats      daemon/pkg/stats
git mv daemon/internal/server     daemon/pkg/server
git mv daemon/internal/pairings   daemon/pkg/pairings
git mv daemon/internal/routines   daemon/pkg/routines
git mv daemon/internal/discovery  daemon/pkg/discovery
git mv daemon/internal/cli        daemon/pkg/cli
rmdir daemon/internal
```

Expected: no errors. `ls daemon/pkg` shows all seven directories.

- [ ] **Step 1.2: Rewrite imports**

Run from repo root:
```
grep -rl 'cydmonitor/daemon/internal' daemon/ \
  | xargs sed -i '' 's|cydmonitor/daemon/internal|cydmonitor/daemon/pkg|g'
```

Expected: no errors. `grep -r 'daemon/internal' daemon/` returns nothing.

- [ ] **Step 1.3: Verify the daemon still builds and tests pass**

Run:
```
cd daemon && go build ./... && go test ./...
```

Expected: all packages compile, all tests pass.

- [ ] **Step 1.4: Commit**

Run from repo root:
```
git add -A
git commit -m "daemon: promote internal packages to pkg for cross-module import

The new desktop/ Wails module needs to import claudedata/stats/server/
pairings/routines/discovery/cli. Go's internal/ visibility blocks that
across module boundaries, so we relocate. No code changes, only paths."
```

---

## Task 2: Extract service assembly into pkg/runner

`daemon/cmd/cydmonitor/main.go` currently does several jobs that the Wails app will also need to do verbatim: build the pairings store, build the records cache + reload loop, build the aggregator, build the server, start mDNS, start the HTTP listener, await context cancellation. We pull all of that into a reusable `runner` package so both binaries assemble the same stack.

**Files:**
- Create: `daemon/pkg/runner/runner.go`
- Create: `daemon/pkg/runner/runner_test.go`
- Modify: `daemon/cmd/cydmonitor/main.go`

- [ ] **Step 2.1: Write the failing test**

Create `daemon/pkg/runner/runner_test.go`:

```go
package runner_test

import (
	"context"
	"net/http"
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/krizdingus/cydmonitor/daemon/pkg/claudedata"
	"github.com/krizdingus/cydmonitor/daemon/pkg/runner"
)

func TestService_StartAndStop(t *testing.T) {
	dir := t.TempDir()
	projects := filepath.Join(dir, "projects")
	if err := os.MkdirAll(projects, 0o755); err != nil {
		t.Fatal(err)
	}
	pairingsPath := filepath.Join(dir, "pairings.json")

	svc, err := runner.New(runner.Options{
		ListenAddr:     "127.0.0.1:0", // ask kernel for any free port
		ProjectsDir:    projects,
		PairingsPath:   pairingsPath,
		PlanInfo:       claudedata.PlanInfo{Plan: claudedata.PlanFree},
		Caps:           claudedata.PlanCaps(claudedata.PlanFree),
		ReloadInterval: 10 * time.Millisecond,
		Version:        "test",
	})
	if err != nil {
		t.Fatalf("New: %v", err)
	}

	ctx, cancel := context.WithCancel(context.Background())
	t.Cleanup(cancel)

	errCh := make(chan error, 1)
	go func() { errCh <- svc.Start(ctx) }()

	// Wait for the listener to come up
	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) {
		if svc.Addr() != "" {
			break
		}
		time.Sleep(5 * time.Millisecond)
	}
	if svc.Addr() == "" {
		t.Fatal("server didn't bind a port within 2s")
	}

	resp, err := http.Get("http://" + svc.Addr() + "/v1/status")
	if err != nil {
		t.Fatalf("GET /v1/status: %v", err)
	}
	resp.Body.Close()
	if resp.StatusCode != 200 {
		t.Fatalf("status %d, want 200", resp.StatusCode)
	}

	cancel()
	select {
	case err := <-errCh:
		if err != nil && err != context.Canceled {
			t.Fatalf("Start returned %v, want nil or context.Canceled", err)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("Service.Start didn't return within 2s of cancel")
	}
}
```

- [ ] **Step 2.2: Run test to verify it fails**

Run:
```
cd daemon && go test ./pkg/runner/...
```

Expected: FAIL with `no such package github.com/krizdingus/cydmonitor/daemon/pkg/runner` or `runner.New undefined`.

- [ ] **Step 2.3: Create runner.go**

Create `daemon/pkg/runner/runner.go`:

```go
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

// Options configures a Service. Required fields:
//   ListenAddr, ProjectsDir, PairingsPath, PlanInfo, Caps, Version.
// Optional:
//   ReloadInterval (default 30s), AdvertiseName (default "Claude Monitor"),
//   Logger (default os.Stderr).
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

// Service is the assembled stack. After New(), call Start(ctx) which blocks
// until ctx is cancelled or a fatal error occurs. Addr() returns the bound
// listener address once Start has had a chance to bind.
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

// Health reflects the current state of the service. Watchers (e.g. the tray)
// poll this via Service.Health().
type Health struct {
	ListenerUp      bool
	PairedCount     int
	LastReloadOK    bool
	LastReloadAt    time.Time
	RecordsCount    int
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
		Routines:   routines.NewFetcher(60 * time.Second),
		Now:        time.Now,
	}
	// Aggregator currently calls claudedata.PlanCaps(opts.PlanInfo.Plan)
	// internally; we'll switch it to accept Caps explicitly in Task 4.
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

// Start blocks until ctx is cancelled. The HTTP listener, reload loop, and
// mDNS advertiser all run as supervised goroutines and shut down on ctx.Done.
func (s *Service) Start(ctx context.Context) error {
	ln, err := net.Listen("tcp", s.opts.ListenAddr)
	if err != nil {
		return fmt.Errorf("listen %s: %w", s.opts.ListenAddr, err)
	}
	s.setAddr(ln.Addr().String())
	s.markListenerUp(true)
	defer s.markListenerUp(false)

	// Initial JSONL scan so /v1/stats has data on the first request.
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
```

- [ ] **Step 2.4: Run test to verify it passes**

Run:
```
cd daemon && go test ./pkg/runner/...
```

Expected: PASS.

- [ ] **Step 2.5: Switch cmd/cydmonitor to use runner**

Replace the body of `daemon/cmd/cydmonitor/main.go` `serve()` with:

```go
func serve() error {
	home, err := os.UserHomeDir()
	if err != nil {
		return err
	}

	claudeDir := filepath.Join(home, ".claude")
	planInfo, _ := claudedata.ReadPlanInfo(filepath.Join(claudeDir, ".claude.json"))

	svc, err := runner.New(runner.Options{
		ListenAddr:    listenAddr,
		ProjectsDir:   filepath.Join(claudeDir, "projects"),
		PairingsPath:  pairingsPath(),
		PlanInfo:      planInfo,
		Caps:          claudedata.PlanCaps(planInfo.Plan),
		Version:       version,
		Logger:        func(f string, a ...any) { fmt.Fprintf(os.Stderr, f+"\n", a...) },
	})
	if err != nil {
		return err
	}

	ctx, cancel := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer cancel()
	return svc.Start(ctx)
}
```

Update imports at the top of the file: drop now-unused (`io/fs`, `path/filepath`'s WalkDir use, `sync`, `discovery`, `pairings`, `routines`, `server`, `stats`) and add `github.com/krizdingus/cydmonitor/daemon/pkg/runner`. Delete the now-dead `recordsCache`, `reloadLoop`, `loadAllJSONL` from this file — they live in runner now.

- [ ] **Step 2.6: Verify daemon still builds, tests + e2e pass**

Run:
```
cd daemon && go build ./... && go test ./...
```

Expected: PASS.

- [ ] **Step 2.7: Commit**

```
git add -A
git commit -m "daemon: extract service assembly into pkg/runner

Both the terminal cmd/cydmonitor and the upcoming Wails desktop app
need to spin up the same pairings + records-cache + reload-loop +
aggregator + server + mDNS stack. Pull it out of main.go into a
runner package that takes a single Options struct. Terminal binary
is now a thin shell over runner.New().Start(ctx).

Also exposes a Health snapshot the tray will poll in a later commit."
```

---

## Task 3: Switch Aggregator to accept Caps explicitly

The aggregator currently calls `claudedata.PlanCaps(a.PlanInfo.Plan)` inside `Build`, which baked the env-var override into the daemon's environment. The desktop app needs to override caps from a JSON file too, so we hoist that responsibility to the caller.

**Files:**
- Modify: `daemon/pkg/stats/aggregator.go`
- Modify: `daemon/pkg/stats/aggregator_test.go`
- Modify: `daemon/pkg/runner/runner.go`

- [ ] **Step 3.1: Update Aggregator struct + Build**

In `daemon/pkg/stats/aggregator.go`, add a `Caps claudedata.Caps` field to `Aggregator` and replace `caps := claudedata.PlanCaps(a.PlanInfo.Plan)` in `Build` with `caps := a.Caps`.

- [ ] **Step 3.2: Run tests to find breakage**

Run:
```
cd daemon && go test ./pkg/stats/...
```

If any tests fail because they previously relied on PlanCaps being called internally, set `agg.Caps = claudedata.PlanCaps(plan)` in the test setup. Update each failing test.

Expected: PASS after fixes.

- [ ] **Step 3.3: Wire Caps in runner**

In `daemon/pkg/runner/runner.go`, set `Caps: opts.Caps` when constructing `*stats.Aggregator`.

- [ ] **Step 3.4: Verify everything still passes**

Run:
```
cd daemon && go build ./... && go test ./...
```

Expected: PASS.

- [ ] **Step 3.5: Commit**

```
git add -A
git commit -m "stats: take Caps from the caller instead of recomputing

The desktop app overrides caps from a JSON settings file (next commit),
so PlanCaps() can't be the single source of truth inside Build any
more. Hoist it to the caller. Terminal binary computes caps via
claudedata.PlanCaps(plan) in main and hands them in; future desktop
app will read them from settings.json."
```

---

## Task 4: Workspace + desktop module scaffold

Set up the multi-module layout so the Wails project can import the daemon's `pkg/*` packages and Wails CLI tooling has everything it needs.

**Files:**
- Create: `go.work` at repo root
- Create: `desktop/` via `wails init`

- [ ] **Step 4.1: Initialize the Wails project**

Run from repo root:
```
wails init -n CYDMonitor -t vanilla -d desktop
```

Expected: `desktop/` is created with the standard Wails v2 layout (main.go, app.go, wails.json, frontend/, build/, go.mod). The generated `desktop/go.mod` will declare a module named `cydmonitor` or similar — we'll rename it next.

- [ ] **Step 4.2: Rename the module**

Open `desktop/go.mod` and change the first line to:
```
module github.com/krizdingus/cydmonitor/desktop
```

If `desktop/main.go` or `desktop/app.go` has any `import` paths referencing the old generated name, sed them to the new name. The freshly scaffolded files don't usually self-reference, so this is just a precaution.

Run:
```
cd desktop && go mod tidy
```

Expected: no errors.

- [ ] **Step 4.3: Create the workspace file**

Create `go.work` at repo root:
```
go 1.23

use (
	./daemon
	./desktop
)
```

- [ ] **Step 4.4: Verify both modules see each other**

Run from repo root:
```
go build ./daemon/... ./desktop/...
```

Expected: both build. (The default Wails scaffold doesn't import the daemon yet — that comes in Task 6.)

- [ ] **Step 4.5: Smoke-test the unmodified Wails scaffold**

Run:
```
cd desktop && wails build
open build/bin/CYDMonitor.app
```

Expected: a blank Wails window opens with the default scaffold content. Close it.

- [ ] **Step 4.6: Commit**

```
git add go.work desktop/
git commit -m "desktop: scaffold Wails v2 project + go workspace

wails init -t vanilla under desktop/, renamed module to
github.com/krizdingus/cydmonitor/desktop, linked via go.work at the
repo root. No daemon integration yet — that lands next."
```

---

## Task 5: Settings package

The settings package owns the JSON file at `~/Library/Application Support/CYDMonitor/settings.json` and produces the `claudedata.Caps` the runner consumes. Precedence: explicit-override field in the file > matching CYDMONITOR_* env var > plan-tier default from `claudedata.PlanCaps`.

**Files:**
- Create: `desktop/internal/settings/settings.go`
- Create: `desktop/internal/settings/settings_test.go`

- [ ] **Step 5.1: Write the failing tests**

Create `desktop/internal/settings/settings_test.go`:

```go
package settings_test

import (
	"os"
	"path/filepath"
	"testing"

	"github.com/krizdingus/cydmonitor/daemon/pkg/claudedata"
	"github.com/krizdingus/cydmonitor/desktop/internal/settings"
)

func TestLoad_MissingFileReturnsDefaults(t *testing.T) {
	path := filepath.Join(t.TempDir(), "settings.json")
	s, err := settings.Load(path)
	if err != nil {
		t.Fatalf("Load: %v", err)
	}
	if s.PlanTier != "max-5x" {
		t.Errorf("PlanTier = %q, want max-5x (default)", s.PlanTier)
	}
	if !s.LaunchAtLogin {
		t.Errorf("LaunchAtLogin = false, want true (default)")
	}
	if len(s.EnabledScreens) == 0 {
		t.Errorf("EnabledScreens empty, want non-empty default list")
	}
}

func TestSaveLoad_RoundTrip(t *testing.T) {
	path := filepath.Join(t.TempDir(), "settings.json")
	want := settings.Settings{
		PlanTier:                  "max-20x",
		WeeklyAllOverride:         100_000_000,
		WeeklyOpusOverride:        20_000_000,
		PollIntervalSeconds:       45,
		EnabledScreens:            []string{"session", "models"},
		LaunchAtLogin:             false,
	}
	if err := settings.Save(path, want); err != nil {
		t.Fatalf("Save: %v", err)
	}
	got, err := settings.Load(path)
	if err != nil {
		t.Fatalf("Load: %v", err)
	}
	if got.PlanTier != want.PlanTier ||
		got.WeeklyAllOverride != want.WeeklyAllOverride ||
		got.WeeklyOpusOverride != want.WeeklyOpusOverride ||
		got.PollIntervalSeconds != want.PollIntervalSeconds ||
		got.LaunchAtLogin != want.LaunchAtLogin ||
		len(got.EnabledScreens) != len(want.EnabledScreens) {
		t.Errorf("got %+v, want %+v", got, want)
	}
}

func TestCaps_FileOverridesEnv(t *testing.T) {
	t.Setenv("CYDMONITOR_WEEKLY_ALL", "12345")
	s := settings.Settings{PlanTier: "max-5x", WeeklyAllOverride: 99_999_999}
	c := s.Caps()
	if c.WeeklyAllModels != 99_999_999 {
		t.Errorf("WeeklyAllModels = %d, want file override 99999999", c.WeeklyAllModels)
	}
}

func TestCaps_EnvOverridesPlanDefault(t *testing.T) {
	t.Setenv("CYDMONITOR_WEEKLY_ALL", "777")
	s := settings.Settings{PlanTier: "max-5x"}
	c := s.Caps()
	if c.WeeklyAllModels != 777 {
		t.Errorf("WeeklyAllModels = %d, want env override 777", c.WeeklyAllModels)
	}
}

func TestCaps_FallsBackToPlanDefault(t *testing.T) {
	t.Setenv("CYDMONITOR_WEEKLY_ALL", "")
	t.Setenv("CYDMONITOR_SESSION_TOKENS", "")
	t.Setenv("CYDMONITOR_WEEKLY_OPUS", "")
	t.Setenv("CYDMONITOR_DAILY_CHAT_MESSAGES", "")
	s := settings.Settings{PlanTier: "max-5x"}
	c := s.Caps()
	want := claudedata.PlanCaps(claudedata.PlanMax5x)
	if c != want {
		t.Errorf("Caps = %+v, want plan default %+v", c, want)
	}
}
```

- [ ] **Step 5.2: Run tests to verify they fail**

Run:
```
cd desktop && go test ./internal/settings/...
```

Expected: FAIL with "no Go files" or "package settings: cannot find".

- [ ] **Step 5.3: Implement settings.go**

Create `desktop/internal/settings/settings.go`:

```go
// Package settings owns the desktop app's persistent configuration
// (~/Library/Application Support/CYDMonitor/settings.json on mac) and
// produces the claudedata.Caps the runner consumes. Precedence for cap
// values: file override > matching CYDMONITOR_* env var > plan default.
package settings

import (
	"encoding/json"
	"errors"
	"io/fs"
	"os"
	"path/filepath"
	"strconv"

	"github.com/krizdingus/cydmonitor/daemon/pkg/claudedata"
)

// Settings is the on-disk schema. A zero value for a cap-override field
// means "no override at this layer, fall through to env/plan default."
type Settings struct {
	PlanTier                  string   `json:"plan_tier"`
	SessionTokensOverride     int      `json:"session_tokens_override"`
	WeeklyAllOverride         int      `json:"weekly_all_override"`
	WeeklyOpusOverride        int      `json:"weekly_opus_override"`
	DailyChatMessagesOverride int      `json:"daily_chat_messages_override"`
	PollIntervalSeconds       int      `json:"poll_interval_seconds"`
	EnabledScreens            []string `json:"enabled_screens"`
	LaunchAtLogin             bool     `json:"launch_at_login"`
}

func defaults() Settings {
	return Settings{
		PlanTier:            "max-5x",
		PollIntervalSeconds: 30,
		EnabledScreens:      []string{"home", "session", "models", "sonnet", "routines", "budgets"},
		LaunchAtLogin:       true,
	}
}

// Load reads settings.json. Missing file returns defaults (and does not error).
// Malformed JSON returns an error; the app should surface it to the user
// rather than silently clobbering their config.
func Load(path string) (Settings, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		if errors.Is(err, fs.ErrNotExist) {
			return defaults(), nil
		}
		return Settings{}, err
	}
	out := defaults()
	if err := json.Unmarshal(data, &out); err != nil {
		return Settings{}, err
	}
	if out.PlanTier == "" {
		out.PlanTier = "max-5x"
	}
	return out, nil
}

// Save atomically writes settings.json, creating parent directories as needed.
func Save(path string, s Settings) error {
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return err
	}
	data, err := json.MarshalIndent(s, "", "  ")
	if err != nil {
		return err
	}
	tmp := path + ".tmp"
	if err := os.WriteFile(tmp, data, 0o600); err != nil {
		return err
	}
	return os.Rename(tmp, path)
}

// Caps resolves the final cap values for the configured plan, applying file
// overrides first, then matching CYDMONITOR_* env vars, then plan defaults.
func (s Settings) Caps() claudedata.Caps {
	c := claudedata.PlanCaps(claudedata.Plan(s.PlanTier))
	apply := func(target *int, fileVal int, envKey string) {
		if fileVal > 0 {
			*target = fileVal
			return
		}
		if v := envInt(envKey); v > 0 {
			*target = v
		}
	}
	apply(&c.SessionBlockTokens, s.SessionTokensOverride, "CYDMONITOR_SESSION_TOKENS")
	apply(&c.WeeklyAllModels, s.WeeklyAllOverride, "CYDMONITOR_WEEKLY_ALL")
	apply(&c.WeeklyOpusOnly, s.WeeklyOpusOverride, "CYDMONITOR_WEEKLY_OPUS")
	apply(&c.DailyChatMessages, s.DailyChatMessagesOverride, "CYDMONITOR_DAILY_CHAT_MESSAGES")
	return c
}

func envInt(key string) int {
	raw := os.Getenv(key)
	if raw == "" {
		return 0
	}
	n, err := strconv.Atoi(raw)
	if err != nil {
		return 0
	}
	return n
}

// DefaultPath returns the platform-specific settings.json path.
func DefaultPath() (string, error) {
	dir, err := os.UserConfigDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(dir, "CYDMonitor", "settings.json"), nil
}
```

Note: `os.UserConfigDir()` returns `~/Library/Application Support` on macOS and `%AppData%` on Windows, which matches the spec.

`PlanCaps` still reads CYDMONITOR_* internally (we did not remove that yet). The override precedence in this function is: file > env > plan default. Because `PlanCaps` already applies the env-var override on top of the plan default before we read it, `c.WeeklyAllModels` will already reflect the env value if any. We then overwrite with the file value if set. This is functionally identical to "file > env > default" because env is folded into the default. Verify in tests.

- [ ] **Step 5.4: Run tests to verify they pass**

Run:
```
cd desktop && go test ./internal/settings/...
```

Expected: PASS.

- [ ] **Step 5.5: Commit**

```
git add desktop/internal/settings/
git commit -m "desktop: settings package (JSON file + Caps resolution)

Owns ~/Library/Application Support/CYDMonitor/settings.json on mac.
Caps() resolves the final cap values: file override > CYDMONITOR_*
env var > plan-tier default from claudedata.PlanCaps."
```

---

## Task 6: Status watcher

A small goroutine that snapshots `runner.Health()` and translates it to a color the tray can subscribe to. Tested with a fake runner — no AppKit needed.

**Files:**
- Create: `desktop/internal/status/status.go`
- Create: `desktop/internal/status/status_test.go`

- [ ] **Step 6.1: Write the failing test**

Create `desktop/internal/status/status_test.go`:

```go
package status_test

import (
	"testing"
	"time"

	"github.com/krizdingus/cydmonitor/daemon/pkg/runner"
	"github.com/krizdingus/cydmonitor/desktop/internal/status"
)

type fakeProbe struct{ h runner.Health }

func (f *fakeProbe) Health() runner.Health { return f.h }

func TestColor_GreenWhenListenerUpAndReloadOK(t *testing.T) {
	got := status.From(runner.Health{
		ListenerUp:   true,
		LastReloadOK: true,
		PairedCount:  1,
		LastReloadAt: time.Now(),
	})
	if got != status.Green {
		t.Errorf("Color = %v, want Green", got)
	}
}

func TestColor_YellowWhenNoPairings(t *testing.T) {
	got := status.From(runner.Health{
		ListenerUp:   true,
		LastReloadOK: true,
		PairedCount:  0,
		LastReloadAt: time.Now(),
	})
	if got != status.Yellow {
		t.Errorf("Color = %v, want Yellow", got)
	}
}

func TestColor_YellowWhenReloadStale(t *testing.T) {
	got := status.From(runner.Health{
		ListenerUp:   true,
		LastReloadOK: true,
		PairedCount:  1,
		LastReloadAt: time.Now().Add(-2 * time.Minute),
	})
	if got != status.Yellow {
		t.Errorf("Color = %v, want Yellow (stale reload)", got)
	}
}

func TestColor_RedWhenListenerDown(t *testing.T) {
	got := status.From(runner.Health{ListenerUp: false})
	if got != status.Red {
		t.Errorf("Color = %v, want Red", got)
	}
}

func TestColor_RedWhenReloadFailed(t *testing.T) {
	got := status.From(runner.Health{
		ListenerUp:   true,
		LastReloadOK: false,
		LastReloadAt: time.Now(),
	})
	if got != status.Red {
		t.Errorf("Color = %v, want Red", got)
	}
}
```

- [ ] **Step 6.2: Run tests to verify they fail**

Run:
```
cd desktop && go test ./internal/status/...
```

Expected: FAIL with "package status: cannot find".

- [ ] **Step 6.3: Implement status.go**

Create `desktop/internal/status/status.go`:

```go
// Package status maps runner.Health snapshots to a tray color and provides
// a Watcher goroutine the UI subscribes to.
package status

import (
	"context"
	"sync"
	"time"

	"github.com/krizdingus/cydmonitor/daemon/pkg/runner"
)

type Color int

const (
	Red Color = iota
	Yellow
	Green
)

func (c Color) String() string {
	switch c {
	case Green:
		return "green"
	case Yellow:
		return "yellow"
	default:
		return "red"
	}
}

// staleAfter is how long since the last successful reload we tolerate before
// going yellow. 90s gives the 30s reload loop two complete missed cycles.
const staleAfter = 90 * time.Second

// From summarizes a Health snapshot. Pure function so it's trivially tested.
func From(h runner.Health) Color {
	if !h.ListenerUp {
		return Red
	}
	if !h.LastReloadOK && !h.LastReloadAt.IsZero() {
		return Red
	}
	if h.PairedCount == 0 {
		return Yellow
	}
	if !h.LastReloadAt.IsZero() && time.Since(h.LastReloadAt) > staleAfter {
		return Yellow
	}
	return Green
}

// Probe is the minimal interface the watcher needs from runner.Service.
type Probe interface {
	Health() runner.Health
}

// Watcher polls a Probe every Interval and emits the latest Color on Updates.
// Construct with NewWatcher, then call Run(ctx) in a goroutine.
type Watcher struct {
	probe    Probe
	interval time.Duration

	mu      sync.RWMutex
	last    Color
	subs    []chan Color
}

func NewWatcher(p Probe, interval time.Duration) *Watcher {
	if interval <= 0 {
		interval = 2 * time.Second
	}
	return &Watcher{probe: p, interval: interval, last: Red}
}

// Last returns the most recently computed color (Red until the first tick).
func (w *Watcher) Last() Color {
	w.mu.RLock()
	defer w.mu.RUnlock()
	return w.last
}

func (w *Watcher) Subscribe() <-chan Color {
	ch := make(chan Color, 1)
	w.mu.Lock()
	w.subs = append(w.subs, ch)
	w.mu.Unlock()
	return ch
}

func (w *Watcher) Run(ctx context.Context) {
	t := time.NewTicker(w.interval)
	defer t.Stop()
	w.tick()
	for {
		select {
		case <-ctx.Done():
			return
		case <-t.C:
			w.tick()
		}
	}
}

func (w *Watcher) tick() {
	c := From(w.probe.Health())
	w.mu.Lock()
	changed := c != w.last
	w.last = c
	subs := append([]chan Color(nil), w.subs...)
	w.mu.Unlock()
	if !changed {
		return
	}
	for _, ch := range subs {
		select {
		case ch <- c:
		default:
		}
	}
}
```

- [ ] **Step 6.4: Run tests to verify they pass**

Run:
```
cd desktop && go test ./internal/status/...
```

Expected: PASS.

- [ ] **Step 6.5: Commit**

```
git add desktop/internal/status/
git commit -m "desktop: status package (Health -> Color watcher)

Pure Color(Health) function for the tray's status indicator, plus a
Watcher goroutine that polls runner.Service.Health() and pushes
changes to subscribers."
```

---

## Task 7: Launch-at-login (macOS)

Toggle launch-at-login by writing/removing `~/Library/LaunchAgents/com.krizdingus.cydmonitor.plist`. The plist starts the .app via `open`, which keeps the dock behavior consistent with the GUI launch path.

**Files:**
- Create: `desktop/internal/loginitem/loginitem_darwin.go`
- Create: `desktop/internal/loginitem/loginitem_darwin_test.go`
- Create: `desktop/internal/loginitem/loginitem_other.go`

- [ ] **Step 7.1: Stub for non-darwin**

Create `desktop/internal/loginitem/loginitem_other.go`:

```go
//go:build !darwin

package loginitem

import "errors"

var ErrUnsupported = errors.New("launch-at-login not supported on this platform")

func Enable(appPath string) error  { return ErrUnsupported }
func Disable() error               { return ErrUnsupported }
func IsEnabled() (bool, error)     { return false, nil }
```

- [ ] **Step 7.2: Write the failing darwin test**

Create `desktop/internal/loginitem/loginitem_darwin_test.go`:

```go
//go:build darwin

package loginitem

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

// PlistPathForTest is wired by the implementation to allow tests to override
// the LaunchAgents location.
func TestEnable_WritesPlist(t *testing.T) {
	dir := t.TempDir()
	t.Setenv("CYDMONITOR_LAUNCHAGENTS_DIR", dir)

	app := "/Applications/CYDMonitor.app"
	if err := Enable(app); err != nil {
		t.Fatalf("Enable: %v", err)
	}
	path := filepath.Join(dir, "com.krizdingus.cydmonitor.plist")
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("plist not written: %v", err)
	}
	if !strings.Contains(string(data), app) {
		t.Errorf("plist missing app path %q:\n%s", app, data)
	}
	if !strings.Contains(string(data), "RunAtLoad") {
		t.Errorf("plist missing RunAtLoad key:\n%s", data)
	}
}

func TestDisable_RemovesPlist(t *testing.T) {
	dir := t.TempDir()
	t.Setenv("CYDMONITOR_LAUNCHAGENTS_DIR", dir)
	if err := Enable("/Applications/CYDMonitor.app"); err != nil {
		t.Fatalf("Enable: %v", err)
	}
	if err := Disable(); err != nil {
		t.Fatalf("Disable: %v", err)
	}
	if _, err := os.Stat(filepath.Join(dir, "com.krizdingus.cydmonitor.plist")); !os.IsNotExist(err) {
		t.Errorf("plist still present after Disable: err=%v", err)
	}
}

func TestIsEnabled_ReflectsFileExistence(t *testing.T) {
	dir := t.TempDir()
	t.Setenv("CYDMONITOR_LAUNCHAGENTS_DIR", dir)
	enabled, err := IsEnabled()
	if err != nil || enabled {
		t.Fatalf("before enable: enabled=%v err=%v", enabled, err)
	}
	if err := Enable("/Applications/CYDMonitor.app"); err != nil {
		t.Fatal(err)
	}
	enabled, err = IsEnabled()
	if err != nil || !enabled {
		t.Fatalf("after enable: enabled=%v err=%v", enabled, err)
	}
}
```

- [ ] **Step 7.3: Implement loginitem_darwin.go**

Create `desktop/internal/loginitem/loginitem_darwin.go`:

```go
//go:build darwin

// Package loginitem toggles launch-at-login on macOS by writing a
// LaunchAgent plist to ~/Library/LaunchAgents. The plist launches the
// .app via `open`, matching Finder's launch behavior.
package loginitem

import (
	"fmt"
	"os"
	"path/filepath"
)

const Label = "com.krizdingus.cydmonitor"

func plistDir() (string, error) {
	if dir := os.Getenv("CYDMONITOR_LAUNCHAGENTS_DIR"); dir != "" {
		return dir, nil
	}
	home, err := os.UserHomeDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(home, "Library", "LaunchAgents"), nil
}

func plistPath() (string, error) {
	dir, err := plistDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(dir, Label+".plist"), nil
}

const plistTemplate = `<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>Label</key>
	<string>%s</string>
	<key>ProgramArguments</key>
	<array>
		<string>/usr/bin/open</string>
		<string>%s</string>
	</array>
	<key>RunAtLoad</key>
	<true/>
</dict>
</plist>
`

func Enable(appPath string) error {
	path, err := plistPath()
	if err != nil {
		return err
	}
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return err
	}
	content := fmt.Sprintf(plistTemplate, Label, appPath)
	return os.WriteFile(path, []byte(content), 0o644)
}

func Disable() error {
	path, err := plistPath()
	if err != nil {
		return err
	}
	if err := os.Remove(path); err != nil && !os.IsNotExist(err) {
		return err
	}
	return nil
}

func IsEnabled() (bool, error) {
	path, err := plistPath()
	if err != nil {
		return false, err
	}
	_, err = os.Stat(path)
	if os.IsNotExist(err) {
		return false, nil
	}
	if err != nil {
		return false, err
	}
	return true, nil
}
```

- [ ] **Step 7.4: Run tests**

Run:
```
cd desktop && go test ./internal/loginitem/...
```

Expected: PASS on macOS.

- [ ] **Step 7.5: Commit**

```
git add desktop/internal/loginitem/
git commit -m "desktop: macOS launch-at-login via LaunchAgent plist

Writes ~/Library/LaunchAgents/com.krizdingus.cydmonitor.plist when
enabled; removes it when disabled. The plist launches the .app via
/usr/bin/open so the dock state matches a Finder launch.

Linux/Windows stubs return ErrUnsupported so cross-builds work."
```

---

## Task 8: Wails app wiring (services + window + JS bindings)

Replace the scaffold-generated `desktop/main.go` and `desktop/app.go` with a real wiring that loads settings, builds the runner, exposes a tiny JS API, and opens a 480×320 window. Tray comes in Task 9.

**Files:**
- Modify: `desktop/main.go`
- Modify: `desktop/app.go`
- Modify: `desktop/wails.json` (set window size and dimensions)
- Modify: `desktop/frontend/index.html`, `desktop/frontend/main.js`, `desktop/frontend/style.css`

- [ ] **Step 8.1: Rewrite app.go**

Replace `desktop/app.go` entirely with:

```go
package main

import (
	"context"
	"fmt"

	"github.com/krizdingus/cydmonitor/daemon/pkg/runner"
)

// App is the bridge Wails binds to JS. Methods here are callable from the
// frontend as `window.go.main.App.<MethodName>()`.
type App struct {
	ctx     context.Context
	svc     *runner.Service
}

func NewApp(svc *runner.Service) *App { return &App{svc: svc} }

func (a *App) startup(ctx context.Context) { a.ctx = ctx }

// GetStatus returns a short JSON-able struct the frontend renders on load.
type StatusResp struct {
	ListenerAddr string `json:"listener_addr"`
	Color        string `json:"color"`
	PairedCount  int    `json:"paired_count"`
}

func (a *App) GetStatus() StatusResp {
	h := a.svc.Health()
	color := "red"
	switch {
	case !h.ListenerUp:
		color = "red"
	case h.PairedCount == 0:
		color = "yellow"
	default:
		color = "green"
	}
	return StatusResp{
		ListenerAddr: a.svc.Addr(),
		Color:        color,
		PairedCount:  h.PairedCount,
	}
}

// OpenSettings is a placeholder the tray "Open settings…" will call;
// in Plan C it'll route to a real settings page. For now: no-op.
func (a *App) OpenSettings() { fmt.Println("OpenSettings invoked (placeholder)") }
```

- [ ] **Step 8.2: Rewrite main.go**

Replace `desktop/main.go` entirely with:

```go
package main

import (
	"context"
	"embed"
	"fmt"
	"os"
	"path/filepath"

	"github.com/wailsapp/wails/v2"
	"github.com/wailsapp/wails/v2/pkg/options"
	"github.com/wailsapp/wails/v2/pkg/options/assetserver"

	"github.com/krizdingus/cydmonitor/daemon/pkg/claudedata"
	"github.com/krizdingus/cydmonitor/daemon/pkg/runner"
	"github.com/krizdingus/cydmonitor/desktop/internal/settings"
)

//go:embed all:frontend
var assets embed.FS

const version = "0.2.0-dev"

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, "fatal:", err)
		os.Exit(1)
	}
}

func run() error {
	home, err := os.UserHomeDir()
	if err != nil {
		return err
	}

	settingsPath, err := settings.DefaultPath()
	if err != nil {
		return err
	}
	s, err := settings.Load(settingsPath)
	if err != nil {
		return fmt.Errorf("settings: %w", err)
	}

	planInfo := claudedata.PlanInfo{Plan: claudedata.Plan(s.PlanTier)}
	svc, err := runner.New(runner.Options{
		ListenAddr:   "0.0.0.0:7842",
		ProjectsDir:  filepath.Join(home, ".claude", "projects"),
		PairingsPath: filepath.Join(home, ".config", "cydmonitor", "pairings.json"),
		PlanInfo:     planInfo,
		Caps:         s.Caps(),
		Version:      version,
	})
	if err != nil {
		return err
	}

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// Service runs for the lifetime of the app.
	go func() {
		if err := svc.Start(ctx); err != nil {
			fmt.Fprintln(os.Stderr, "service:", err)
		}
	}()

	app := NewApp(svc)
	return wails.Run(&options.App{
		Title:  "CYDMonitor",
		Width:  480,
		Height: 320,
		AssetServer: &assetserver.Options{
			Assets: assets,
		},
		OnStartup:  app.startup,
		OnShutdown: func(_ context.Context) { cancel() },
		Bind:       []interface{}{app},
	})
}
```

- [ ] **Step 8.3: Frontend placeholder**

Replace `desktop/frontend/index.html`:

```html
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8" />
    <title>CYDMonitor</title>
    <link rel="stylesheet" href="style.css" />
  </head>
  <body>
    <main>
      <h1>CYDMonitor</h1>
      <p id="status">Loading…</p>
      <p class="hint">Settings UI lands in a future update.</p>
    </main>
    <script src="main.js"></script>
  </body>
</html>
```

Replace `desktop/frontend/main.js`:

```js
const { GetStatus } = window.go.main.App;

async function refresh() {
  const s = await GetStatus();
  document.getElementById("status").textContent =
    `Daemon listening on http://${s.listener_addr} — ${s.paired_count} paired device(s) — status: ${s.color}`;
}

refresh();
setInterval(refresh, 5000);
```

Replace `desktop/frontend/style.css`:

```css
:root { color-scheme: light dark; font-family: -apple-system, sans-serif; }
body { margin: 0; padding: 24px; }
main { max-width: 420px; }
h1 { font-size: 18px; margin: 0 0 12px; }
.hint { color: #888; font-size: 12px; }
```

- [ ] **Step 8.4: Build and visually verify**

Run:
```
cd desktop && wails build && open build/bin/CYDMonitor.app
```

Expected: a 480×320 window appears showing "Daemon listening on http://0.0.0.0:7842 — N paired device(s) — status: …". Close it.

Then verify the embedded daemon is live (in another terminal):
```
TOKEN=$(grep -m1 '"token"' ~/.config/cydmonitor/pairings.json | sed 's/.*"\([0-9a-f]*\)".*/\1/')
curl -s -H "Authorization: Bearer $TOKEN" http://127.0.0.1:7842/v1/stats | head -c 200
```

Expected: JSON beginning with `{"schema":1,...`.

If the terminal binary is also running on :7842, stop it first (one process per port).

- [ ] **Step 8.5: Commit**

```
git add desktop/main.go desktop/app.go desktop/frontend/
git commit -m "desktop: wire Wails app to embed the daemon

main.go reads settings.json, constructs runner.Options, starts the
service in a goroutine, opens a 480x320 window. app.go exposes
GetStatus/OpenSettings to the JS frontend. Placeholder index.html
shows the listener addr + paired-device count and refreshes every
5s. Tray and launch-at-login UI land in the next two tasks."
```

---

## Task 9: System tray

Add a tray icon driven by `getlantern/systray`, with a menu the spec defines: app name + version, status indicator, "Open settings…", "Add a device…" (disabled), "Quit". `systray.Run` must be on the main thread, so we run it via Wails' `OnStartup` -> goroutine -> `runtime.LockOSThread()` pattern.

**Files:**
- Modify: `desktop/main.go`
- Modify: `desktop/app.go`
- Create: `desktop/internal/tray/tray.go`
- Create: `desktop/internal/tray/tray_test.go`
- Create: `desktop/build/icon_green.png`, `icon_yellow.png`, `icon_red.png` — three 18×18 monochrome PNG icons (a colored dot is enough for v1).

- [ ] **Step 9.1: Add the systray dependency**

Run:
```
cd desktop && go get github.com/getlantern/systray@latest
```

Expected: `go.mod` gains the dependency.

- [ ] **Step 9.2: Drop in placeholder icons**

Place three 18×18 PNGs under `desktop/build/`:
- `icon_green.png` — solid green circle on transparent bg
- `icon_yellow.png` — solid yellow
- `icon_red.png` — solid red

A trivial way to generate them (run once, not per build):
```
brew install imagemagick
for c in green yellow red; do magick -size 18x18 xc:transparent \
  -fill $c -draw 'circle 9,9 9,3' desktop/build/icon_$c.png; done
```

(Replace with hand-drawn icons later if you care.)

- [ ] **Step 9.3: Embed icons + write tray controller (with tests)**

Create `desktop/internal/tray/tray.go`:

```go
// Package tray drives the macOS menu-bar icon. The Controller is the testable
// surface; Run wires it to getlantern/systray which is not unit-testable.
package tray

import (
	"context"
	_ "embed"
	"sync"

	"github.com/getlantern/systray"

	"github.com/krizdingus/cydmonitor/desktop/internal/status"
)

//go:embed icons/icon_green.png
var iconGreen []byte

//go:embed icons/icon_yellow.png
var iconYellow []byte

//go:embed icons/icon_red.png
var iconRed []byte

// Actions are the menu callbacks the host wires in.
type Actions struct {
	OpenSettings func()
	AddDevice    func() // nil = disabled
	Quit         func()
}

// iconFor returns the bytes for the given color.
func iconFor(c status.Color) []byte {
	switch c {
	case status.Green:
		return iconGreen
	case status.Yellow:
		return iconYellow
	default:
		return iconRed
	}
}

// Run blocks (must be called from the main goroutine). It builds the menu,
// installs initial icon, and listens on the watcher for color changes.
func Run(ctx context.Context, watcher *status.Watcher, version string, a Actions) {
	onReady := func() {
		systray.SetIcon(iconFor(watcher.Last()))
		systray.SetTitle("")
		systray.SetTooltip("CYDMonitor")

		mTitle := systray.AddMenuItem("CYDMonitor v"+version, "")
		mTitle.Disable()
		mStatus := systray.AddMenuItem("Status: "+watcher.Last().String(), "")
		mStatus.Disable()
		systray.AddSeparator()
		mSettings := systray.AddMenuItem("Open settings…", "")
		mAdd := systray.AddMenuItem("Add a device…", "")
		mAdd.Disable() // wired in Plan B
		systray.AddSeparator()
		mQuit := systray.AddMenuItem("Quit", "")

		subs := watcher.Subscribe()

		var wg sync.WaitGroup
		wg.Add(1)
		go func() {
			defer wg.Done()
			for {
				select {
				case <-ctx.Done():
					return
				case c := <-subs:
					systray.SetIcon(iconFor(c))
					mStatus.SetTitle("Status: " + c.String())
				case <-mSettings.ClickedCh:
					if a.OpenSettings != nil {
						a.OpenSettings()
					}
				case <-mAdd.ClickedCh:
					if a.AddDevice != nil {
						a.AddDevice()
					}
				case <-mQuit.ClickedCh:
					if a.Quit != nil {
						a.Quit()
					}
					systray.Quit()
					return
				}
			}
		}()
		<-ctx.Done()
		systray.Quit()
		wg.Wait()
	}
	onExit := func() {}
	systray.Run(onReady, onExit)
}
```

Move (or copy) the three icon PNGs into `desktop/internal/tray/icons/`:
```
mkdir -p desktop/internal/tray/icons
cp desktop/build/icon_*.png desktop/internal/tray/icons/
```

- [ ] **Step 9.4: Write tray test (controller logic only)**

Create `desktop/internal/tray/tray_test.go`:

```go
package tray

import (
	"testing"

	"github.com/krizdingus/cydmonitor/desktop/internal/status"
)

func TestIconFor(t *testing.T) {
	if len(iconFor(status.Green)) == 0 {
		t.Error("iconFor(Green) empty")
	}
	if len(iconFor(status.Yellow)) == 0 {
		t.Error("iconFor(Yellow) empty")
	}
	if len(iconFor(status.Red)) == 0 {
		t.Error("iconFor(Red) empty")
	}
}
```

The systray run loop is exercised manually; we don't try to unit-test the click channels.

- [ ] **Step 9.5: Wire tray into main.go**

In `desktop/main.go`, replace the `wails.Run(&options.App{...})` call with a setup that constructs the watcher and starts the tray on the main thread *before* wails is launched on a goroutine. The exact wiring:

```go
import (
	// ... existing imports
	"github.com/krizdingus/cydmonitor/desktop/internal/status"
	"github.com/krizdingus/cydmonitor/desktop/internal/tray"
)

// inside run() after svc.Start() goroutine:
	watcher := status.NewWatcher(svc, 2*time.Second)
	go watcher.Run(ctx)

	app := NewApp(svc)
	app.openSettings = func() { /* Wails Show window — see below */ }

	// Wails on a goroutine, tray on the main thread.
	wailsErr := make(chan error, 1)
	go func() {
		wailsErr <- wails.Run(&options.App{ ... same as before ... })
	}()

	tray.Run(ctx, watcher, version, tray.Actions{
		OpenSettings: func() { app.OpenSettings() },
		Quit:         func() { cancel() },
	})

	return <-wailsErr
```

Add `"time"` to imports.

In `desktop/app.go`, add an `openSettings func()` field so the tray can ask the window to show. For now, `OpenSettings()` just prints; the real "show window" hook comes when Plan C lands a settings page.

- [ ] **Step 9.6: Build + visual verification**

Run:
```
cd desktop && wails build && open build/bin/CYDMonitor.app
```

Expected:
- Tray icon appears (colored dot) in the menu bar.
- Right-clicking shows: CYDMonitor v0.2.0-dev (disabled), Status: <color>, Open settings… (enabled), Add a device… (disabled), Quit (enabled).
- Quitting via the tray menu actually exits the app (window AND tray both go away).
- Closing the window leaves the tray alive.

- [ ] **Step 9.7: Run tests**

Run:
```
cd desktop && go test ./...
```

Expected: PASS.

- [ ] **Step 9.8: Commit**

```
git add desktop/
git commit -m "desktop: system tray with status indicator

getlantern/systray, three placeholder colored-dot icons embedded with
go:embed. Menu: app name (disabled), status line (disabled),
Open settings, Add a device (disabled for now), Quit. status.Watcher
drives the icon color; subscribers update on every change."
```

---

## Task 10: Wire settings.LaunchAtLogin to loginitem

When the app starts, reconcile the launch-agent plist with `settings.LaunchAtLogin`. Expose a JS-callable method to flip it at runtime so the (future) settings UI can toggle it.

**Files:**
- Modify: `desktop/main.go`
- Modify: `desktop/app.go`

- [ ] **Step 10.1: Reconcile at startup**

In `desktop/main.go`, after `Load(settingsPath)`, add:

```go
appPath, _ := os.Executable()
// appPath points at the binary inside the .app; walk up to the .app bundle.
if i := strings.Index(appPath, ".app/"); i >= 0 {
	appPath = appPath[:i+4]
}
if s.LaunchAtLogin {
	_ = loginitem.Enable(appPath)
} else {
	_ = loginitem.Disable()
}
```

Add imports for `"strings"` and `"github.com/krizdingus/cydmonitor/desktop/internal/loginitem"`.

- [ ] **Step 10.2: Expose JS-callable toggle**

In `desktop/app.go`, add:

```go
import (
	// ... existing imports
	"github.com/krizdingus/cydmonitor/desktop/internal/loginitem"
)

func (a *App) SetLaunchAtLogin(enabled bool) error {
	appPath, err := os.Executable()
	if err != nil {
		return err
	}
	if i := strings.Index(appPath, ".app/"); i >= 0 {
		appPath = appPath[:i+4]
	}
	if enabled {
		return loginitem.Enable(appPath)
	}
	return loginitem.Disable()
}

func (a *App) GetLaunchAtLogin() (bool, error) {
	return loginitem.IsEnabled()
}
```

Add the matching imports (`"os"`, `"strings"`).

- [ ] **Step 10.3: Build + smoke**

Run:
```
cd desktop && wails build && open build/bin/CYDMonitor.app
```

Then from a terminal:
```
ls ~/Library/LaunchAgents/com.krizdingus.cydmonitor.plist
```

Expected: present if `settings.LaunchAtLogin` is true (the default). Delete it and relaunch with `LaunchAtLogin: false` in settings.json to verify Disable runs.

- [ ] **Step 10.4: Commit**

```
git add -A
git commit -m "desktop: reconcile launch-at-login from settings on startup

settings.LaunchAtLogin drives a one-shot Enable/Disable on each
launch. App methods SetLaunchAtLogin/GetLaunchAtLogin are bound to
JS so the future settings UI can flip it."
```

---

## Task 11: End-to-end smoke + Makefile targets

Add a smoke test that builds the .app, launches it headless-friendly enough to hit /v1/stats, and tears it down. Add convenience Makefile targets.

**Files:**
- Modify: `daemon/Makefile`
- Create: `desktop/Makefile`

- [ ] **Step 11.1: Add desktop Makefile**

Create `desktop/Makefile`:

```make
.PHONY: build dev test clean

build:
	wails build

dev:
	wails dev

test:
	go test ./...

clean:
	rm -rf build/bin
```

- [ ] **Step 11.2: Add cross-cutting targets at daemon/Makefile**

Append to `daemon/Makefile`:

```make
desktop-build:
	$(MAKE) -C ../desktop build

desktop-test:
	$(MAKE) -C ../desktop test

desktop-dev:
	$(MAKE) -C ../desktop dev
```

- [ ] **Step 11.3: Manual smoke checklist**

Run these steps in order; each must succeed:

```
# 1. Daemon binary still works.
cd daemon && go test ./... && go build -o bin/cydmonitor ./cmd/cydmonitor

# 2. Stop any running cydmonitor (terminal binary OR previous app build).
pkill -f cydmonitor || true

# 3. Build and launch the app.
cd ../desktop && wails build
open build/bin/CYDMonitor.app

# 4. Tray icon visible? Window visible? Status reads non-error?
# 5. /v1/stats responds with schema 1.
TOKEN=$(grep -m1 '"token"' ~/.config/cydmonitor/pairings.json | sed 's/.*"\([0-9a-f]*\)".*/\1/')
curl -s -H "Authorization: Bearer $TOKEN" http://127.0.0.1:7842/v1/stats | python3 -c 'import sys,json; print(json.load(sys.stdin)["schema"])'
# Expected: 1

# 6. Quitting via the tray actually exits.
osascript -e 'tell application "CYDMonitor" to quit'
pgrep -f CYDMonitor && echo FAIL || echo CLEAN_EXIT
# Expected: CLEAN_EXIT
```

- [ ] **Step 11.4: Commit**

```
git add daemon/Makefile desktop/Makefile
git commit -m "build: Makefile targets for desktop build/dev/test

Cross-makefile convenience: desktop-build / desktop-test / desktop-dev
from the daemon dir delegate into the desktop project so the dev loop
mirrors the existing terminal flow."
```

---

## Task 12: Update CLAUDE-facing docs

Quick docs sweep so future you (or another contributor) knows where things live.

**Files:**
- Modify: `daemon/README.md` (note that `desktop/` exists)
- Create: `desktop/README.md`

- [ ] **Step 12.1: desktop/README.md**

Write a short README explaining:
- What this directory is (the Wails app)
- Prerequisites (Go 1.23, Wails CLI v2.12.0+)
- Build: `make build`
- Dev hot-reload: `make dev`
- Settings file location
- That Plans B (flasher + USB provisioner) and C (full settings UI + auto-update + Windows) are still pending

Keep it under 60 lines. Plain prose, no marketing.

- [ ] **Step 12.2: Touch daemon/README.md**

Add a one-paragraph note near the top:

> **Phase 3b:** the user-facing entrypoint is the Wails desktop app at `../desktop`. This binary is still supported for terminal/Linux users and for development; it now reads its configuration from CYDMONITOR_* env vars only (the desktop app uses a JSON settings file).

- [ ] **Step 12.3: Commit**

```
git add daemon/README.md desktop/README.md
git commit -m "docs: note desktop app + cross-link from daemon README"
```

---

## Out of scope (Plans B and C)

The following are intentionally **not** in this plan and should not be added during execution. If the executor finds themselves writing any of these, they should stop and confirm scope with the user.

- Esptool subprocess wrapper and the `Flasher` component.
- USB serial port enumeration, `Provisioner` JSON sending, "Add a device" wizard UI.
- Embedding firmware artifacts (`bootloader.bin`, `partitions.bin`, `firmware.bin`).
- A full settings UI (plan tier dropdown, override numeric inputs, screen toggles, etc.) — the current placeholder page stays a placeholder until Plan C.
- An About page or update-check flow.
- Sparkle/appcast auto-update.
- DMG packaging, NSIS installer, code signing, notarization.
- Windows support of any kind (build tags, paths, plist alternatives).
- Removing the 4-digit pairing-code endpoints from the daemon (they stay in the server for now; we just don't use them from the desktop path).

When Plan B starts, it builds on the `App` struct's bind surface, adding methods like `EnumeratePorts`, `FlashAndProvision`, etc. When Plan C starts, the frontend grows real pages and an auto-update path goes in.

---

## Self-review checklist (executor: skim before declaring done)

1. `make test` (in `daemon/`) still passes.
2. `go test ./...` (in `desktop/`) passes.
3. `cd daemon && go build ./...` succeeds.
4. `cd desktop && wails build` produces a launchable .app.
5. App launches, window appears, tray icon appears.
6. `/v1/stats` returns valid schema-v1 JSON via the running app's daemon.
7. Closing window does not exit; tray Quit does exit.
8. `~/Library/LaunchAgents/com.krizdingus.cydmonitor.plist` present when `LaunchAtLogin: true`.
9. Editing `~/Library/Application Support/CYDMonitor/settings.json` and relaunching changes the caps reflected in `/v1/stats`.
10. The terminal `./daemon/bin/cydmonitor` still runs and serves the same endpoints (one process per port at a time — they cannot run simultaneously, by design).
