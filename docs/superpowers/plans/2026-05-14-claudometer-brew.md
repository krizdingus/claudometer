# Claudometer: rename + brew install Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rename the project from `claudometer` to `claudometer` and ship it as a Homebrew formula that registers a background service on macOS *and* Linux through `brew services`. After this plan, `brew install krizdingus/tap/claudometer && brew services start claudometer` produces a running daemon with auto-start on login on either OS, with zero per-OS Go code on our side. A JSON config at `~/.config/claudometer/config.json` lets users set plan tier and cap overrides without rebuilding.

**Architecture:** Homebrew's `service` block in the formula tells brew how to run the daemon; brew writes the launchd plist (macOS) or systemd user unit (Linux) for us. We just provide a clean Go binary. The new `daemon/pkg/config` package owns the JSON config and produces the `claudedata.Caps` the runner consumes (file override > matching CLAUDOMETER_* env var > plan-tier default). The brew formula lives in a separate `github.com/krizdingus/homebrew-tap` repo; the formula source-builds the binary from this repo at install time so we don't need GoReleaser or signed binaries for v1.

**Tech Stack:** Go 1.23 (stdlib only — no new daemon deps). Homebrew formula in Ruby. Pre-built release binaries deferred — formula does `go build` at install time, with Go as a build-time dep.

---

## Why this shape

The earlier Wails app pivot (`feat/desktop-app-core`, abandoned and reverted) ran into a wall on macOS: getlantern/systray and Wails v2 both want the main thread for NSApp, and a tray icon never appeared. More importantly, the user does not want a foreground GUI process at all. The right shape for "set it up, forget it" is a background service the OS auto-starts at login.

Earlier this plan called for our own platform-specific installer code (launchd plist generator for darwin, systemd unit generator for linux). That was a bad call — Homebrew's `service` block on the formula already handles both platforms (`brew services` on Linux uses `systemd --user` under the hood, and `launchd` on macOS). We get cross-platform service registration for free, with one formula.

The rename to `claudometer` is bundled into this plan because every commit going forward should use the new name, and a single sweep is cheaper than a slow drift.

## What success looks like

After this plan:

1. `brew tap krizdingus/tap` + `brew install claudometer` produces `/opt/homebrew/bin/claudometer` (macOS Apple Silicon) or equivalent for Intel/Linux.
2. `brew services start claudometer` registers and starts the daemon. On macOS, `launchctl list | grep claudometer` shows it. On Linux, `systemctl --user status claudometer` shows it.
3. The service auto-starts on login (brew configures this).
4. `~/.config/claudometer/config.json` is created on first run with sensible defaults (`plan_tier: "free"`, `listen_addr: "0.0.0.0:7842"`). Editing `plan_tier` and restarting the service via `brew services restart claudometer` changes `/v1/stats` caps accordingly.
5. `~/.config/claudometer/pairings.json` (renamed from `claudometer`) stores paired CYD bearer tokens. Existing users with `~/.config/claudometer/pairings.json` get a one-time auto-migration on first claudometer launch.
6. `CLAUDOMETER_*` env vars work as fallbacks when the config file omits a field, mirroring the previous `CLAUDOMETER_*` behavior. (The old prefix is dropped; the user is the only existing user and is doing the rename themselves.)
7. `brew services stop claudometer` and `brew uninstall claudometer` cleanly tear down without orphaning the plist/unit.
8. The terminal `claudometer` binary (without brew) still runs: `./bin/claudometer` from a dev checkout, just like `./bin/claudometer` did. No mandatory dependency on brew.
9. All daemon tests pass.
10. The Go module path is `github.com/krizdingus/claudometer/daemon`. The GitHub repo can be renamed from `cyd-claude-usage-monitor` to `claudometer` as a separate manual GitHub action after this plan lands.

What is **not** in scope: a Windows installer, pre-built signed release binaries (GoReleaser/notarization), a `claudometer flash <port>` subcommand for CYD provisioning (manual `screen`/`pyserial` flow stays documented for now), a browser-based setup UI.

## File structure

**Renamed (Task 1):**
- `daemon/cmd/claudometer/` → `daemon/cmd/claudometer/`
- All imports `github.com/krizdingus/claudometer/daemon/pkg/...` → `github.com/krizdingus/claudometer/daemon/pkg/...`
- `daemon/go.mod` module path updated.
- All occurrences of the literal string `claudometer` in code and docs swept to `claudometer`. All `CLAUDOMETER_*` env vars to `CLAUDOMETER_*`.

**Created (Tasks 2–4):**
- `daemon/pkg/config/config.go` — `Settings` struct, `Load(path)`, `Save(path)`, `EnsureExists(path)`, `(s Settings) Caps() claudedata.Caps`, `DefaultPath() (string, error)`.
- `daemon/pkg/config/config_test.go` — load/save round-trip, override-precedence tests.
- `daemon/pkg/config/migrate.go` + tests — one-time migration of `~/.config/claudometer/pairings.json` → `~/.config/claudometer/pairings.json` on first run.
- A separate `homebrew-tap` repo (created on GitHub) — see Task 4 for the formula file content.

**Modified:**
- `daemon/cmd/claudometer/main.go` — read config file via `config.Load` before the existing env-var path; pass `cfg.ListenAddr` and `cfg.Caps()` into `runner.Options`.
- `daemon/README.md` — replace install instructions; document `brew install krizdingus/tap/claudometer` flow; manual CYD provisioning section unchanged in spirit but updated for the new name.
- `daemon/Makefile` — binary name change.

**Deleted:**
- Nothing in the daemon. The Wails plan doc was already removed in a prior commit.

---

## Task 1: Rename claudometer → claudometer

A mechanical sweep. Module path, binary, paths, env vars, docs. Do this first so every subsequent task uses the new name.

**Files:**
- Modify: every Go file that imports `github.com/krizdingus/claudometer/daemon/...`
- Modify: every file referencing the literal `claudometer` (config paths, env var names, etc.)
- Move: `daemon/cmd/claudometer/` → `daemon/cmd/claudometer/`
- Modify: `daemon/go.mod`, `daemon/Makefile`
- Modify: docs that reference claudometer

- [ ] **Step 1.1: Move the cmd directory**

```bash
cd /Volumes/Storage/Dev/cyd-claude-usage-monitor
git mv daemon/cmd/claudometer daemon/cmd/claudometer
```

- [ ] **Step 1.2: Update the module path**

Edit `daemon/go.mod`. Change the first line:
```
module github.com/krizdingus/claudometer/daemon
```
to:
```
module github.com/krizdingus/claudometer/daemon
```

- [ ] **Step 1.3: Rewrite Go imports**

```bash
grep -rl 'github.com/krizdingus/claudometer' daemon/ | xargs sed -i '' \
  's|github.com/krizdingus/claudometer|github.com/krizdingus/claudometer|g'
```

Verify nothing was missed:
```bash
grep -rn 'krizdingus/claudometer' daemon/   # expect: nothing
```

- [ ] **Step 1.4: Rewrite user-visible strings**

This is the trickier sweep because we want to replace `claudometer` and `CYDMONITOR` in source/text but NOT inadvertently touch unrelated bytes. Run two passes:

```bash
# Lowercase: claudometer → claudometer
grep -rl 'claudometer' daemon/ | xargs sed -i '' 's|claudometer|claudometer|g'
# Uppercase env var prefix: CLAUDOMETER_ → CLAUDOMETER_
grep -rl 'CLAUDOMETER_' daemon/ | xargs sed -i '' 's|CLAUDOMETER_|CLAUDOMETER_|g'
```

Then check for the (now-renamed) plan/symbol called `PlanCYDMONITOR` or similar — there is none in this codebase, but a final grep is cheap insurance:
```bash
grep -rn -i 'claudometer' daemon/   # expect: nothing
```

- [ ] **Step 1.5: Update Makefile**

In `daemon/Makefile`, the `build` target outputs to `bin/claudometer`. After step 1.4's sed pass it should already say `bin/claudometer`. Confirm:
```bash
grep claudometer daemon/Makefile
```
Expected: at least the `build` line.

- [ ] **Step 1.6: Update docs**

Sweep the rest of the repo (docs, READMEs, plan files):
```bash
grep -rl 'claudometer' docs/ firmware/ README.md 2>/dev/null | xargs sed -i '' 's|claudometer|claudometer|g' 2>/dev/null
grep -rl 'CLAUDOMETER_' docs/ firmware/ README.md 2>/dev/null | xargs sed -i '' 's|CLAUDOMETER_|CLAUDOMETER_|g' 2>/dev/null
```

**Exception:** Leave the historical plan docs (`docs/superpowers/plans/2026-05-13-*.md`) and the old design spec (`docs/superpowers/specs/2026-05-13-cyd-claude-usage-monitor-design.md`) alone — they are historical record. If the sweep above touched them, revert them:
```bash
git checkout HEAD -- docs/superpowers/plans/2026-05-13-daemon-mvp.md
git checkout HEAD -- docs/superpowers/plans/2026-05-13-firmware-mvp.md
git checkout HEAD -- docs/superpowers/plans/2026-05-13-firmware-usb-provisioning.md
git checkout HEAD -- docs/superpowers/specs/2026-05-13-cyd-claude-usage-monitor-design.md
```

- [ ] **Step 1.7: Verify build + tests**

```bash
cd daemon && go build ./... && go test ./...
```

All packages compile, all tests pass. The `bin/claudometer` binary will no longer be produced (we now build `bin/claudometer`); old artifact can be deleted with `rm -f bin/claudometer`.

- [ ] **Step 1.8: Commit**

```bash
cd /Volumes/Storage/Dev/cyd-claude-usage-monitor
git add -A
git commit -m "rename: claudometer → claudometer

Module path, binary, command directory, env var prefix, config and
pairings paths, and all user-visible references. Historical design
docs and plan files are deliberately left as-is (they describe the
old name at the time they were written).

The GitHub repo itself stays cyd-claude-usage-monitor for now; rename
to claudometer is a separate manual step."
```

(No "Co-Authored-By" trailer, no emoji.)

---

## Task 2: Config package

**Files:**
- Create: `daemon/pkg/config/config.go`
- Create: `daemon/pkg/config/config_test.go`

- [ ] **Step 2.1: Write the failing tests**

Create `daemon/pkg/config/config_test.go`:

```go
package config_test

import (
	"path/filepath"
	"testing"

	"github.com/krizdingus/claudometer/daemon/pkg/claudedata"
	"github.com/krizdingus/claudometer/daemon/pkg/config"
)

func TestLoad_MissingFileReturnsDefaults(t *testing.T) {
	path := filepath.Join(t.TempDir(), "config.json")
	s, err := config.Load(path)
	if err != nil {
		t.Fatalf("Load: %v", err)
	}
	if s.PlanTier != "free" {
		t.Errorf("PlanTier = %q, want free (default)", s.PlanTier)
	}
	if s.ListenAddr != "0.0.0.0:7842" {
		t.Errorf("ListenAddr = %q, want 0.0.0.0:7842", s.ListenAddr)
	}
}

func TestSaveLoad_RoundTrip(t *testing.T) {
	path := filepath.Join(t.TempDir(), "config.json")
	want := config.Settings{
		PlanTier:           "max-5x",
		WeeklyAllOverride:  45_000_000,
		WeeklyOpusOverride: 9_000_000,
		ListenAddr:         "127.0.0.1:7842",
	}
	if err := config.Save(path, want); err != nil {
		t.Fatalf("Save: %v", err)
	}
	got, err := config.Load(path)
	if err != nil {
		t.Fatalf("Load: %v", err)
	}
	if got.PlanTier != want.PlanTier ||
		got.WeeklyAllOverride != want.WeeklyAllOverride ||
		got.WeeklyOpusOverride != want.WeeklyOpusOverride ||
		got.ListenAddr != want.ListenAddr {
		t.Errorf("got %+v, want %+v", got, want)
	}
}

func TestCaps_FileOverridesEnv(t *testing.T) {
	t.Setenv("CLAUDOMETER_WEEKLY_ALL", "12345")
	s := config.Settings{PlanTier: "max-5x", WeeklyAllOverride: 99_999_999}
	c := s.Caps()
	if c.WeeklyAllModels != 99_999_999 {
		t.Errorf("WeeklyAllModels = %d, want file override 99999999", c.WeeklyAllModels)
	}
}

func TestCaps_EnvOverridesPlanDefault(t *testing.T) {
	t.Setenv("CLAUDOMETER_WEEKLY_ALL", "777")
	s := config.Settings{PlanTier: "max-5x"}
	c := s.Caps()
	if c.WeeklyAllModels != 777 {
		t.Errorf("WeeklyAllModels = %d, want env override 777", c.WeeklyAllModels)
	}
}

func TestCaps_FallsBackToPlanDefault(t *testing.T) {
	t.Setenv("CLAUDOMETER_WEEKLY_ALL", "")
	t.Setenv("CLAUDOMETER_SESSION_TOKENS", "")
	t.Setenv("CLAUDOMETER_WEEKLY_OPUS", "")
	t.Setenv("CLAUDOMETER_DAILY_CHAT_MESSAGES", "")
	s := config.Settings{PlanTier: "max-5x"}
	c := s.Caps()
	want := claudedata.PlanCaps(claudedata.PlanMax5x)
	if c != want {
		t.Errorf("Caps = %+v, want plan default %+v", c, want)
	}
}

func TestEnsureExists_CreatesDefaultFile(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "claudometer", "config.json")
	s, err := config.EnsureExists(path)
	if err != nil {
		t.Fatalf("EnsureExists: %v", err)
	}
	if s.PlanTier != "free" {
		t.Errorf("PlanTier = %q, want free", s.PlanTier)
	}
	// File must exist after EnsureExists.
	if _, err := config.Load(path); err != nil {
		t.Errorf("Load after EnsureExists: %v", err)
	}
}
```

- [ ] **Step 2.2: Run tests to verify they fail**

```bash
cd daemon && go test ./pkg/config/...
```

Expected: FAIL (package not found).

- [ ] **Step 2.3: Implement config.go**

Create `daemon/pkg/config/config.go`:

```go
// Package config owns the daemon's persistent configuration at
// ~/.config/claudometer/config.json (both macOS and Linux). Caps() produces
// the claudedata.Caps the runner consumes: file override > matching
// CLAUDOMETER_* env var > plan-tier default from claudedata.PlanCaps.
package config

import (
	"encoding/json"
	"errors"
	"io/fs"
	"os"
	"path/filepath"
	"strconv"

	"github.com/krizdingus/claudometer/daemon/pkg/claudedata"
)

// Settings is the on-disk schema. A zero value for a cap-override field
// means "no override at this layer, fall through to env/plan default."
type Settings struct {
	PlanTier                  string `json:"plan_tier"`
	SessionTokensOverride     int    `json:"session_tokens_override"`
	WeeklyAllOverride         int    `json:"weekly_all_override"`
	WeeklyOpusOverride        int    `json:"weekly_opus_override"`
	DailyChatMessagesOverride int    `json:"daily_chat_messages_override"`
	ListenAddr                string `json:"listen_addr"`
}

func defaults() Settings {
	return Settings{
		PlanTier:   "free",
		ListenAddr: "0.0.0.0:7842",
	}
}

// Load reads config.json. Missing file returns defaults (and does not error).
// Malformed JSON returns an error.
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
		out.PlanTier = "free"
	}
	if out.ListenAddr == "" {
		out.ListenAddr = "0.0.0.0:7842"
	}
	return out, nil
}

// Save atomically writes config.json, creating parent directories as needed.
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

// EnsureExists creates a default config file at path if it does not exist.
// Returns the resolved Settings (whether freshly written or already present).
func EnsureExists(path string) (Settings, error) {
	s, err := Load(path)
	if err != nil {
		return Settings{}, err
	}
	if _, statErr := os.Stat(path); errors.Is(statErr, fs.ErrNotExist) {
		if err := Save(path, s); err != nil {
			return Settings{}, err
		}
	}
	return s, nil
}

// Caps resolves the final cap values for the configured plan, applying file
// overrides first, then matching CLAUDOMETER_* env vars, then plan defaults.
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
	apply(&c.SessionBlockTokens, s.SessionTokensOverride, "CLAUDOMETER_SESSION_TOKENS")
	apply(&c.WeeklyAllModels, s.WeeklyAllOverride, "CLAUDOMETER_WEEKLY_ALL")
	apply(&c.WeeklyOpusOnly, s.WeeklyOpusOverride, "CLAUDOMETER_WEEKLY_OPUS")
	apply(&c.DailyChatMessages, s.DailyChatMessagesOverride, "CLAUDOMETER_DAILY_CHAT_MESSAGES")
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

// DefaultPath returns ~/.config/claudometer/config.json (macOS and Linux).
// We use the XDG-style location on macOS rather than ~/Library/Application
// Support so the pairings file and the config file live next to each other
// and Linux users get the same path.
func DefaultPath() (string, error) {
	home, err := os.UserHomeDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(home, ".config", "claudometer", "config.json"), nil
}
```

Also note: the existing `claudedata.PlanCaps` already reads `CLAUDOMETER_*` env vars internally. **After Task 1's rename**, those reads now use `CLAUDOMETER_*` because the sed swept them. Verify that's the case:
```bash
grep -n 'CLAUDOMETER_\|CLAUDOMETER_' daemon/pkg/claudedata/planinfo.go
```
Expected: every match is `CLAUDOMETER_`.

- [ ] **Step 2.4: Run tests to verify they pass**

```bash
cd daemon && go test ./pkg/config/...
```

Expected: PASS (6 tests).

- [ ] **Step 2.5: Commit**

```bash
git add daemon/pkg/config/
git commit -m "daemon: add config package (JSON file + Caps resolver)

~/.config/claudometer/config.json owns plan tier, cap overrides, and
listen addr. Caps() resolves final values: file override > matching
CLAUDOMETER_* env var > plan default. EnsureExists() seeds a default
file on first launch so users can configure by editing one file."
```

---

## Task 3: Wire config into main + pairings migration

**Files:**
- Modify: `daemon/cmd/claudometer/main.go`

- [ ] **Step 3.1: Replace env-only setup with config-file flow**

In `daemon/cmd/claudometer/main.go`, modify `serve()`:

```go
func serve() error {
	home, err := os.UserHomeDir()
	if err != nil {
		return err
	}

	configPath, err := config.DefaultPath()
	if err != nil {
		return err
	}
	cfg, err := config.EnsureExists(configPath)
	if err != nil {
		return fmt.Errorf("config: %w", err)
	}

	// One-shot migration: move ~/.config/claudometer/pairings.json to the new
	// claudometer path if the user is upgrading. Silently no-ops otherwise.
	migratePairings(home)

	claudeDir := filepath.Join(home, ".claude")
	planInfo, _ := claudedata.ReadPlanInfo(filepath.Join(claudeDir, ".claude.json"))
	if cfg.PlanTier != "" {
		planInfo.Plan = claudedata.Plan(cfg.PlanTier)
	}

	svc, err := runner.New(runner.Options{
		ListenAddr:   cfg.ListenAddr,
		ProjectsDir:  filepath.Join(claudeDir, "projects"),
		PairingsPath: pairingsPath(),
		PlanInfo:     planInfo,
		Caps:         cfg.Caps(),
		Version:      version,
		Logger:       func(f string, a ...any) { fmt.Fprintf(os.Stderr, f+"\n", a...) },
	})
	if err != nil {
		return err
	}

	ctx, cancel := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer cancel()
	return svc.Start(ctx)
}

// migratePairings copies ~/.config/claudometer/pairings.json to
// ~/.config/claudometer/pairings.json if the new path is missing but the old
// one exists. After a successful copy it does NOT delete the old file —
// leaving it gives users a way back if they downgrade, and removing it
// requires us to be sure the new daemon is fully working. They can delete
// the old path themselves when comfortable.
func migratePairings(home string) {
	newPath := filepath.Join(home, ".config", "claudometer", "pairings.json")
	oldPath := filepath.Join(home, ".config", "claudometer", "pairings.json")
	if _, err := os.Stat(newPath); err == nil {
		return // new already present, nothing to do
	}
	if _, err := os.Stat(oldPath); err != nil {
		return // old not present either; fresh install
	}
	data, err := os.ReadFile(oldPath)
	if err != nil {
		return
	}
	if err := os.MkdirAll(filepath.Dir(newPath), 0o755); err != nil {
		return
	}
	_ = os.WriteFile(newPath, data, 0o600)
}
```

Update the `pairingsPath()` function in the same file to return the new path:

```go
func pairingsPath() string {
	home, _ := os.UserHomeDir()
	return filepath.Join(home, ".config", "claudometer", "pairings.json")
}
```

(After Task 1, the sed should have already updated this. Confirm.)

Add imports: `"github.com/krizdingus/claudometer/daemon/pkg/config"`. Remove the `listenAddr` const (config now provides it) — check whether the e2e test references it; if so, hardcode `"127.0.0.1:7842"` there.

- [ ] **Step 3.2: Verify build + tests**

```bash
cd daemon && go build ./... && go test ./...
```

All packages must pass. The e2e test in `cmd/claudometer/main_e2e_test.go` binds :7842; ensure no other process holds it (`pkill -f claudometer || pkill -f claudometer || true`).

- [ ] **Step 3.3: Commit**

```bash
git add -A
git commit -m "daemon: read config from ~/.config/claudometer/config.json

serve() now seeds config.json on first run via EnsureExists, then
builds runner.Options from it. CLAUDOMETER_* env vars still work as
overrides. ListenAddr is now a config value, not a constant.

Includes a one-shot migration of ~/.config/claudometer/pairings.json
to the new claudometer path so existing pairings survive the rename
without re-pairing the CYD."
```

---

## Task 4: Homebrew formula

This task touches a SEPARATE repository (`github.com/krizdingus/homebrew-tap`). The formula file is small enough that you can write it inline; the executor needs to (a) create the tap repo on GitHub if it doesn't exist, and (b) push the formula to it.

**Files:**
- Create (in `homebrew-tap` repo): `Formula/claudometer.rb`

- [ ] **Step 4.1: Create the tap repo (manual, GitHub)**

This step is one the user does themselves in the GitHub web UI (or the executor does via `gh`):
```bash
gh repo create krizdingus/homebrew-tap --public --description "Homebrew tap for krizdingus' projects"
```
A naming requirement: the repo MUST be named `homebrew-<tapname>` for `brew tap krizdingus/<tapname>` to find it. The default is `homebrew-tap` which gives `brew tap krizdingus/tap`. Keep that.

- [ ] **Step 4.2: Clone the tap repo locally**

Pick a sibling directory to the main repo, e.g. `/Volumes/Storage/Dev/homebrew-tap`. Don't put it inside the claudometer repo.

```bash
gh repo clone krizdingus/homebrew-tap /Volumes/Storage/Dev/homebrew-tap
cd /Volumes/Storage/Dev/homebrew-tap
mkdir -p Formula
```

- [ ] **Step 4.3: Write Formula/claudometer.rb**

Create `/Volumes/Storage/Dev/homebrew-tap/Formula/claudometer.rb`:

```ruby
class Claudometer < Formula
  desc "Claude usage monitor that drives a Cheap Yellow Display"
  homepage "https://github.com/krizdingus/claudometer"
  url "https://github.com/krizdingus/cyd-claude-usage-monitor.git",
      branch: "main"
  version "0.1.0"
  license "MIT"
  head "https://github.com/krizdingus/cyd-claude-usage-monitor.git", branch: "main"

  depends_on "go" => :build

  def install
    cd "daemon" do
      system "go", "build", *std_go_args(output: bin/"claudometer"), "./cmd/claudometer"
    end
  end

  service do
    run [opt_bin/"claudometer"]
    keep_alive true
    log_path var/"log/claudometer.log"
    error_log_path var/"log/claudometer.err.log"
  end

  test do
    assert_match "claudometer", shell_output("#{bin}/claudometer version 2>&1", 0).strip
  end
end
```

Notes for the executor:
- `url` and `head` both point at `cyd-claude-usage-monitor` because at the time of writing the GitHub repo has not been renamed. Once the user renames the repo on GitHub, update this formula (separate small PR).
- `depends_on "go" => :build` makes brew install Go transparently if the user doesn't have it. Brew uninstalls the build-time Go after the install completes.
- `service do … end` is the brew-services hook. On macOS this generates a launchd plist; on Linux (Homebrew on Linux) it generates a systemd user unit. Both routes use `keep_alive` to restart on crash.
- `log_path` / `error_log_path` go to brew's variable directory (typically `$HOMEBREW_PREFIX/var/log/`) so the user has somewhere to look.
- The `test do` block runs as part of `brew test claudometer`; it's a smoke check that the binary exists and prints something.

- [ ] **Step 4.4: Commit + push tap**

```bash
cd /Volumes/Storage/Dev/homebrew-tap
git add Formula/claudometer.rb
git commit -m "claudometer 0.1.0 formula"
git push origin main
```

- [ ] **Step 4.5: Smoke test the formula (manual)**

From a clean shell on the user's mac:

```bash
brew tap krizdingus/tap
brew install claudometer
which claudometer                              # expect: $HOMEBREW_PREFIX/bin/claudometer
claudometer version                            # expect: 0.1.0-dev or similar

brew services start claudometer
brew services list | grep claudometer          # expect: started

# Daemon should be up:
sleep 2
ls ~/.config/claudometer/                      # expect: config.json (+ pairings.json if migrated)
cat ~/.config/claudometer/config.json
TOKEN=$(python3 -c "import json; print(json.load(open('$HOME/.config/claudometer/pairings.json'))[0]['token'])")
curl -s -H "Authorization: Bearer $TOKEN" http://127.0.0.1:7842/v1/stats | python3 -m json.tool | head

# Stop + uninstall:
brew services stop claudometer
brew uninstall claudometer
brew untap krizdingus/tap
```

If any step fails, capture the failure mode and re-dispatch with the failing step's output.

- [ ] **Step 4.6: Commit (claudometer repo only — tap repo is separate)**

Nothing committed in the claudometer repo for this task; the formula lives in the tap repo. Skip the commit step here.

---

## Task 5: Documentation

**Files:**
- Modify: `daemon/README.md`
- Optionally modify: top-level `README.md` if one exists

- [ ] **Step 5.1: Rewrite installation section**

`daemon/README.md` should now lead with:

```markdown
## Installation

### Homebrew (recommended, macOS and Linux)

    brew install krizdingus/tap/claudometer
    brew services start claudometer

This builds the binary from source (Go is installed as a transparent
build-time dependency) and registers `claudometer` as a background
service that starts on login. On macOS this uses launchd; on Linux
it uses systemd --user. Stop and restart with `brew services stop|restart claudometer`.

### From source (dev / fallback)

    git clone https://github.com/krizdingus/cyd-claude-usage-monitor.git
    cd cyd-claude-usage-monitor/daemon
    make build
    ./bin/claudometer

This runs in the foreground. To run as a background service without
brew, write a launchd plist (macOS) or systemd unit (Linux) yourself —
see the `service` block in [the formula][formula] for the exact
arguments brew uses.

[formula]: https://github.com/krizdingus/homebrew-tap/blob/main/Formula/claudometer.rb
```

Then a `## Configuration` section covering:
- `~/.config/claudometer/config.json` — fields and what each does
- `CLAUDOMETER_*` env vars as overrides
- How to set plan tier and how to apply (`brew services restart claudometer` or kill + re-run for dev)

Then a `## Pairing a CYD` section pointing at the firmware's USB provisioning protocol (`firmware/src/net/usb_provisioner.h`) and showing the JSON payload to send via `screen` or `pyserial`. Mention that a turnkey `claudometer flash` subcommand is on the roadmap.

Keep the file under 250 lines.

- [ ] **Step 5.2: Commit**

```bash
git add daemon/README.md
git commit -m "docs: brew install flow + config file + manual CYD pairing"
```

---

## Out of scope (future plans)

- **`claudometer flash <port>`** — wrap esptool + provisioning-JSON push. Currently users `screen` into the CYD manually and paste the JSON.
- **Browser-based setup UI** at `http://localhost:7842/setup` — for editing config and triggering the flasher from a browser. Defer until the basic install flow is proven.
- **Pre-built signed release binaries** via GoReleaser + notarytool. The current source-build flow works but takes ~30s to install. Worth replacing once we have actual users.
- **Windows installer.** Separate plan, completely different (Scoop bucket or MSI). Brew on Windows exists but isn't standard.
- **GitHub repo rename** (`cyd-claude-usage-monitor` → `claudometer`). Manual step; one-line update to the formula's `url` after that.

---

## Self-review checklist

1. `cd daemon && go test ./...` — all green
2. `grep -rn 'claudometer\|CYDMONITOR' daemon/` — returns nothing (historical doc files don't count)
3. `~/.config/claudometer/config.json` is created with sensible defaults on first run
4. `~/.config/claudometer/pairings.json` exists after first run (either fresh or migrated from claudometer path)
5. Editing `plan_tier` in config + restarting the service changes `/v1/stats` caps
6. `brew install krizdingus/tap/claudometer` succeeds on macOS
7. `brew services start claudometer` registers the service and the daemon comes up on :7842
8. `brew services stop claudometer && brew uninstall claudometer` cleans up
9. The terminal `./bin/claudometer` still works from a dev checkout (no brew dependency)
10. No new third-party Go dependencies
11. No "Co-Authored-By" trailers in any commits
