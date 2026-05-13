# CYD Claude Monitor — Daemon MVP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the host-side daemon that reads Claude Code data, computes usage stats, and serves them to a paired CYD over HTTP + mDNS.

**Architecture:** Single Go binary, single-process. Reads `~/.claude/projects/*.jsonl` and `~/.claude.json`. Computes session blocks, weekly budgets, today's model breakdown, and cost estimates. Serves a single `/v1/stats` JSON endpoint with bearer-token auth. Advertises itself on the LAN via mDNS (`_claudeusage._tcp.local`). Pairs with a CYD via a 4-digit code printed to the daemon CLI.

**Tech Stack:** Go 1.22+, stdlib `net/http`, `github.com/grandcat/zeroconf` for mDNS, JSONL fixture testing, no external web framework.

**Reference spec:** `docs/superpowers/specs/2026-05-13-cyd-claude-usage-monitor-design.md`

**Out of scope for this plan:**
- Web UI dashboard (later plan)
- launchd / systemd service files (later plan, ships with distribution)
- Anthropic billing API integration (phase 2)
- Claude.ai cookie auth (phase 3)
- The chat-screen data is stubbed with zeroes in this plan; firmware must render empty-state when chat is absent.

---

## File Structure

```
daemon/
  cmd/cydmonitor/
    main.go                          # entry point, CLI dispatch
  internal/
    claudedata/
      jsonl.go                       # Record type + line parser
      jsonl_test.go
      session.go                     # 5-hour block computer
      session_test.go
      budgets.go                     # weekly window math
      budgets_test.go
      models.go                      # today's per-model totals
      models_test.go
      planinfo.go                    # ~/.claude.json reader
      planinfo_test.go
      pricing.go                     # model price table + cost estimator
      pricing_test.go
    routines/
      routines.go                    # shell out to claude routines, cache
      routines_test.go
    stats/
      types.go                       # Stats response struct (the API contract)
      aggregator.go                  # combines everything
      aggregator_test.go
    pairings/
      store.go                       # paired-client token store on disk
      store_test.go
      codes.go                       # pending-pairing code generator
      codes_test.go
    server/
      server.go                      # http.Server wiring
      auth.go                        # bearer token middleware
      auth_test.go
      handlers.go                    # /v1/stats, /v1/pair-*, /v1/status
      handlers_test.go
    discovery/
      mdns.go                        # zeroconf advertiser
    cli/
      status.go                      # `cydmonitor status` subcommand
      reset.go                       # `cydmonitor reset-pairings` subcommand
  testdata/
    claude/
      projects/proj-alpha.jsonl      # fixture session data
      .claude.json                   # fixture plan info
    routines/list-output.json        # fixture routines CLI output
  go.mod
  go.sum
  Makefile
  README.md
```

Each `internal/` package has one responsibility. The `claudedata` package is the parser core. `stats` is the aggregator that turns parsed data into the API response. `server` is HTTP-only — it depends on `stats` but knows nothing about parsing. `pairings` is a self-contained token store.

---

### Task 1: Project scaffold

**Files:**
- Create: `daemon/go.mod`
- Create: `daemon/cmd/cydmonitor/main.go`
- Create: `daemon/Makefile`
- Create: `daemon/.gitignore`

- [ ] **Step 1: Initialize Go module**

```bash
cd daemon
go mod init github.com/krizdingus/cydmonitor/daemon
go get github.com/grandcat/zeroconf@latest
```

- [ ] **Step 2: Create minimal main.go**

```go
// daemon/cmd/cydmonitor/main.go
package main

import (
	"fmt"
	"os"
)

const version = "0.1.0-dev"

func main() {
	if len(os.Args) > 1 {
		switch os.Args[1] {
		case "version":
			fmt.Println(version)
			return
		case "status":
			fmt.Fprintln(os.Stderr, "status: not yet implemented")
			os.Exit(2)
		case "reset-pairings":
			fmt.Fprintln(os.Stderr, "reset-pairings: not yet implemented")
			os.Exit(2)
		}
	}
	fmt.Fprintln(os.Stderr, "serve: not yet implemented")
	os.Exit(2)
}
```

- [ ] **Step 3: Create Makefile**

```makefile
# daemon/Makefile
.PHONY: build test run clean

build:
	go build -o bin/cydmonitor ./cmd/cydmonitor

test:
	go test ./...

run: build
	./bin/cydmonitor

clean:
	rm -rf bin/
```

- [ ] **Step 4: Create .gitignore**

```
# daemon/.gitignore
bin/
*.test
*.out
```

- [ ] **Step 5: Verify build and version**

Run:
```bash
cd daemon && make build && ./bin/cydmonitor version
```
Expected output: `0.1.0-dev`

- [ ] **Step 6: Commit**

```bash
git add daemon/
git commit -m "scaffold daemon Go module"
```

---

### Task 2: JSONL record parser

**Files:**
- Create: `daemon/internal/claudedata/jsonl.go`
- Create: `daemon/internal/claudedata/jsonl_test.go`
- Create: `daemon/testdata/claude/projects/proj-alpha.jsonl`

- [ ] **Step 1: Create fixture JSONL**

```jsonl
{"sessionId":"alpha-1","timestamp":"2026-05-13T09:00:00Z","type":"user","message":{"content":"hi"}}
{"sessionId":"alpha-1","timestamp":"2026-05-13T09:00:02Z","type":"assistant","message":{"id":"msg_1","model":"claude-sonnet-4-6","usage":{"input_tokens":120,"output_tokens":340,"cache_read_input_tokens":5000,"cache_creation_input_tokens":1000}}}
{"sessionId":"alpha-1","timestamp":"2026-05-13T09:30:00Z","type":"assistant","message":{"id":"msg_2","model":"claude-sonnet-4-6","usage":{"input_tokens":80,"output_tokens":120,"cache_read_input_tokens":5800,"cache_creation_input_tokens":0}}}
{"sessionId":"alpha-1","timestamp":"2026-05-13T15:00:00Z","type":"assistant","message":{"id":"msg_3","model":"claude-opus-4-7","usage":{"input_tokens":500,"output_tokens":1200,"cache_read_input_tokens":0,"cache_creation_input_tokens":2000}}}
```

Save to `daemon/testdata/claude/projects/proj-alpha.jsonl`.

- [ ] **Step 2: Write failing test for line parser**

```go
// daemon/internal/claudedata/jsonl_test.go
package claudedata

import (
	"strings"
	"testing"
	"time"
)

func TestParseLine_AssistantWithUsage(t *testing.T) {
	line := `{"sessionId":"alpha-1","timestamp":"2026-05-13T09:00:02Z","type":"assistant","message":{"id":"msg_1","model":"claude-sonnet-4-6","usage":{"input_tokens":120,"output_tokens":340,"cache_read_input_tokens":5000,"cache_creation_input_tokens":1000}}}`
	r, ok, err := ParseLine([]byte(line))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !ok {
		t.Fatalf("expected record to be kept, got skipped")
	}
	if r.SessionID != "alpha-1" {
		t.Errorf("SessionID = %q, want alpha-1", r.SessionID)
	}
	want, _ := time.Parse(time.RFC3339, "2026-05-13T09:00:02Z")
	if !r.Timestamp.Equal(want) {
		t.Errorf("Timestamp = %v, want %v", r.Timestamp, want)
	}
	if r.Model != "claude-sonnet-4-6" {
		t.Errorf("Model = %q", r.Model)
	}
	if r.Tokens.Input != 120 || r.Tokens.Output != 340 ||
		r.Tokens.CacheRead != 5000 || r.Tokens.CacheCreation != 1000 {
		t.Errorf("Tokens = %+v", r.Tokens)
	}
}

func TestParseLine_UserMessageSkipped(t *testing.T) {
	line := `{"sessionId":"alpha-1","timestamp":"2026-05-13T09:00:00Z","type":"user","message":{"content":"hi"}}`
	_, ok, err := ParseLine([]byte(line))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if ok {
		t.Errorf("expected user record to be skipped")
	}
}

func TestParseLine_MalformedReturnsError(t *testing.T) {
	_, _, err := ParseLine([]byte("not json"))
	if err == nil {
		t.Errorf("expected error for malformed line")
	}
}

func TestParseFile_ReadsFixture(t *testing.T) {
	records, err := ParseFile("../../testdata/claude/projects/proj-alpha.jsonl")
	if err != nil {
		t.Fatalf("ParseFile: %v", err)
	}
	if len(records) != 3 {
		t.Errorf("got %d records, want 3 (one user line should be skipped)", len(records))
	}
}

func TestParseFile_SkipsCorruptLines(t *testing.T) {
	corrupt := "not json\n" +
		`{"sessionId":"x","timestamp":"2026-05-13T09:00:00Z","type":"assistant","message":{"id":"m","model":"claude-haiku-4-5","usage":{"input_tokens":1,"output_tokens":1}}}` + "\n"
	records, err := ParseReader(strings.NewReader(corrupt))
	if err != nil {
		t.Fatalf("ParseReader: %v", err)
	}
	if len(records) != 1 {
		t.Errorf("want 1 record (malformed line skipped), got %d", len(records))
	}
}
```

- [ ] **Step 3: Run test to confirm failure**

Run: `cd daemon && go test ./internal/claudedata/...`
Expected: build error — `ParseLine`, `ParseFile`, `ParseReader` undefined.

- [ ] **Step 4: Implement the parser**

```go
// daemon/internal/claudedata/jsonl.go
package claudedata

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"time"
)

type Usage struct {
	Input         int
	Output        int
	CacheRead     int
	CacheCreation int
}

func (u Usage) Total() int {
	return u.Input + u.Output + u.CacheRead + u.CacheCreation
}

type Record struct {
	SessionID string
	Timestamp time.Time
	Model     string
	Tokens    Usage
}

type rawRecord struct {
	SessionID string    `json:"sessionId"`
	Timestamp time.Time `json:"timestamp"`
	Type      string    `json:"type"`
	Message   *struct {
		Model string `json:"model"`
		Usage *struct {
			InputTokens              int `json:"input_tokens"`
			OutputTokens             int `json:"output_tokens"`
			CacheReadInputTokens     int `json:"cache_read_input_tokens"`
			CacheCreationInputTokens int `json:"cache_creation_input_tokens"`
		} `json:"usage"`
	} `json:"message"`
}

// ParseLine returns (record, kept, err). Lines that are valid JSON but represent
// non-assistant or usage-less messages return ok=false with nil error.
func ParseLine(line []byte) (Record, bool, error) {
	var raw rawRecord
	if err := json.Unmarshal(line, &raw); err != nil {
		return Record{}, false, fmt.Errorf("parse: %w", err)
	}
	if raw.Type != "assistant" || raw.Message == nil || raw.Message.Usage == nil {
		return Record{}, false, nil
	}
	return Record{
		SessionID: raw.SessionID,
		Timestamp: raw.Timestamp,
		Model:     raw.Message.Model,
		Tokens: Usage{
			Input:         raw.Message.Usage.InputTokens,
			Output:        raw.Message.Usage.OutputTokens,
			CacheRead:     raw.Message.Usage.CacheReadInputTokens,
			CacheCreation: raw.Message.Usage.CacheCreationInputTokens,
		},
	}, true, nil
}

func ParseReader(r io.Reader) ([]Record, error) {
	scanner := bufio.NewScanner(r)
	scanner.Buffer(make([]byte, 64*1024), 1024*1024)
	var out []Record
	for scanner.Scan() {
		line := scanner.Bytes()
		if len(line) == 0 {
			continue
		}
		rec, ok, err := ParseLine(line)
		if err != nil || !ok {
			continue
		}
		out = append(out, rec)
	}
	return out, scanner.Err()
}

func ParseFile(path string) ([]Record, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	return ParseReader(f)
}
```

- [ ] **Step 5: Run tests to verify pass**

Run: `cd daemon && go test ./internal/claudedata/...`
Expected: all tests PASS.

- [ ] **Step 6: Commit**

```bash
git add daemon/internal/claudedata/jsonl.go daemon/internal/claudedata/jsonl_test.go daemon/testdata/
git commit -m "add JSONL record parser for Claude Code data"
```

---

### Task 3: 5-hour session block computer

**Files:**
- Create: `daemon/internal/claudedata/session.go`
- Create: `daemon/internal/claudedata/session_test.go`

- [ ] **Step 1: Write failing test**

```go
// daemon/internal/claudedata/session_test.go
package claudedata

import (
	"testing"
	"time"
)

func mkRec(ts string, model string, in, out int) Record {
	t, _ := time.Parse(time.RFC3339, ts)
	return Record{Timestamp: t, Model: model, Tokens: Usage{Input: in, Output: out}}
}

func TestComputeBlocks_SingleBlock(t *testing.T) {
	recs := []Record{
		mkRec("2026-05-13T09:00:00Z", "claude-sonnet-4-6", 100, 200),
		mkRec("2026-05-13T10:30:00Z", "claude-sonnet-4-6", 100, 200),
		mkRec("2026-05-13T13:00:00Z", "claude-opus-4-7", 100, 200),
	}
	blocks := ComputeBlocks(recs, 5*time.Hour)
	if len(blocks) != 1 {
		t.Fatalf("got %d blocks, want 1", len(blocks))
	}
	if blocks[0].TokensUsed != 900 {
		t.Errorf("TokensUsed = %d, want 900", blocks[0].TokensUsed)
	}
}

func TestComputeBlocks_GapStartsNewBlock(t *testing.T) {
	// Two messages, then 6 hours of silence, then another message
	recs := []Record{
		mkRec("2026-05-13T09:00:00Z", "claude-sonnet-4-6", 100, 200),
		mkRec("2026-05-13T09:30:00Z", "claude-sonnet-4-6", 100, 200),
		mkRec("2026-05-13T16:00:00Z", "claude-sonnet-4-6", 100, 200),
	}
	blocks := ComputeBlocks(recs, 5*time.Hour)
	if len(blocks) != 2 {
		t.Fatalf("got %d blocks, want 2", len(blocks))
	}
	if len(blocks[0].Records) != 2 {
		t.Errorf("block 0 has %d records, want 2", len(blocks[0].Records))
	}
	if len(blocks[1].Records) != 1 {
		t.Errorf("block 1 has %d records, want 1", len(blocks[1].Records))
	}
}

func TestComputeBlocks_BlockEndsAtStartPlusDuration(t *testing.T) {
	recs := []Record{mkRec("2026-05-13T09:00:00Z", "claude-sonnet-4-6", 100, 200)}
	blocks := ComputeBlocks(recs, 5*time.Hour)
	want, _ := time.Parse(time.RFC3339, "2026-05-13T14:00:00Z")
	if !blocks[0].End.Equal(want) {
		t.Errorf("End = %v, want %v", blocks[0].End, want)
	}
}

func TestComputeBlocks_PerModelTotals(t *testing.T) {
	recs := []Record{
		mkRec("2026-05-13T09:00:00Z", "claude-sonnet-4-6", 100, 200),
		mkRec("2026-05-13T09:10:00Z", "claude-opus-4-7", 50, 75),
		mkRec("2026-05-13T09:20:00Z", "claude-sonnet-4-6", 30, 40),
	}
	blocks := ComputeBlocks(recs, 5*time.Hour)
	got := blocks[0].PerModel
	if got["claude-sonnet-4-6"] != 370 {
		t.Errorf("sonnet total = %d, want 370", got["claude-sonnet-4-6"])
	}
	if got["claude-opus-4-7"] != 125 {
		t.Errorf("opus total = %d, want 125", got["claude-opus-4-7"])
	}
}

func TestActiveBlock_ReturnsNilWhenNoBlocks(t *testing.T) {
	if b := ActiveBlock(nil, time.Now()); b != nil {
		t.Errorf("want nil, got %+v", b)
	}
}

func TestActiveBlock_ReturnsLastIfWithinWindow(t *testing.T) {
	recs := []Record{mkRec("2026-05-13T09:00:00Z", "claude-sonnet-4-6", 100, 200)}
	blocks := ComputeBlocks(recs, 5*time.Hour)
	now, _ := time.Parse(time.RFC3339, "2026-05-13T13:00:00Z")
	if b := ActiveBlock(blocks, now); b == nil {
		t.Errorf("want active block, got nil")
	}
	expired, _ := time.Parse(time.RFC3339, "2026-05-13T15:00:00Z")
	if b := ActiveBlock(blocks, expired); b != nil {
		t.Errorf("want nil after expiry, got %+v", b)
	}
}
```

- [ ] **Step 2: Run test to confirm failure**

Run: `cd daemon && go test ./internal/claudedata/...`
Expected: build error — `ComputeBlocks`, `ActiveBlock`, `Block` undefined.

- [ ] **Step 3: Implement the block computer**

```go
// daemon/internal/claudedata/session.go
package claudedata

import (
	"sort"
	"time"
)

type Block struct {
	Start      time.Time
	End        time.Time
	Records    []Record
	TokensUsed int
	PerModel   map[string]int
}

// ComputeBlocks groups records into time-bounded blocks. Each block starts at
// the timestamp of its first record and ends at start+duration. Records whose
// timestamps fall outside the current block's window open a new block.
func ComputeBlocks(records []Record, duration time.Duration) []Block {
	if len(records) == 0 {
		return nil
	}
	sorted := make([]Record, len(records))
	copy(sorted, records)
	sort.SliceStable(sorted, func(i, j int) bool {
		return sorted[i].Timestamp.Before(sorted[j].Timestamp)
	})

	var blocks []Block
	for _, r := range sorted {
		if len(blocks) == 0 || !r.Timestamp.Before(blocks[len(blocks)-1].End) {
			blocks = append(blocks, Block{
				Start:    r.Timestamp,
				End:      r.Timestamp.Add(duration),
				PerModel: map[string]int{},
			})
		}
		cur := &blocks[len(blocks)-1]
		cur.Records = append(cur.Records, r)
		cur.TokensUsed += r.Tokens.Total()
		cur.PerModel[r.Model] += r.Tokens.Total()
	}
	return blocks
}

// ActiveBlock returns the block whose [Start, End) window contains now,
// or nil if no such block exists.
func ActiveBlock(blocks []Block, now time.Time) *Block {
	for i := range blocks {
		if !now.Before(blocks[i].Start) && now.Before(blocks[i].End) {
			return &blocks[i]
		}
	}
	return nil
}
```

- [ ] **Step 4: Run tests**

Run: `cd daemon && go test ./internal/claudedata/...`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add daemon/internal/claudedata/session.go daemon/internal/claudedata/session_test.go
git commit -m "compute 5-hour session blocks from records"
```

---

### Task 4: Plan info reader

**Files:**
- Create: `daemon/internal/claudedata/planinfo.go`
- Create: `daemon/internal/claudedata/planinfo_test.go`
- Create: `daemon/testdata/claude/.claude.json`

- [ ] **Step 1: Create fixture .claude.json**

```json
{
  "plan": "max-20x",
  "userEmail": "test@example.com",
  "lastUpdated": "2026-05-13T09:00:00Z"
}
```

Save to `daemon/testdata/claude/.claude.json`.

- [ ] **Step 2: Write failing test**

```go
// daemon/internal/claudedata/planinfo_test.go
package claudedata

import "testing"

func TestReadPlanInfo_ParsesFixture(t *testing.T) {
	info, err := ReadPlanInfo("../../testdata/claude/.claude.json")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if info.Plan != PlanMax20x {
		t.Errorf("Plan = %v, want max-20x", info.Plan)
	}
}

func TestReadPlanInfo_MissingFileReturnsFreeDefault(t *testing.T) {
	info, err := ReadPlanInfo("/nonexistent")
	if err != nil {
		t.Fatalf("missing file should not error, got: %v", err)
	}
	if info.Plan != PlanFree {
		t.Errorf("Plan = %v, want free default", info.Plan)
	}
}

func TestPlanCaps_ReturnsKnownLimits(t *testing.T) {
	caps := PlanCaps(PlanMax20x)
	if caps.SessionBlockTokens <= 0 {
		t.Errorf("SessionBlockTokens = %d", caps.SessionBlockTokens)
	}
	if caps.WeeklyAllModels <= 0 {
		t.Errorf("WeeklyAllModels = %d", caps.WeeklyAllModels)
	}
	if caps.WeeklyOpusOnly <= 0 || caps.WeeklyOpusOnly >= caps.WeeklyAllModels {
		t.Errorf("WeeklyOpusOnly = %d, want positive and less than WeeklyAllModels", caps.WeeklyOpusOnly)
	}
}
```

- [ ] **Step 3: Run test to verify failure**

Run: `cd daemon && go test ./internal/claudedata/...`
Expected: build error — `ReadPlanInfo`, `PlanCaps`, plan constants undefined.

- [ ] **Step 4: Implement plan reader**

```go
// daemon/internal/claudedata/planinfo.go
package claudedata

import (
	"encoding/json"
	"errors"
	"io/fs"
	"os"
)

type Plan string

const (
	PlanFree   Plan = "free"
	PlanPro    Plan = "pro"
	PlanMax5x  Plan = "max-5x"
	PlanMax20x Plan = "max-20x"
)

type PlanInfo struct {
	Plan      Plan   `json:"plan"`
	UserEmail string `json:"userEmail"`
}

type Caps struct {
	SessionBlockTokens int // cap per 5-hour block
	WeeklyAllModels    int
	WeeklyOpusOnly     int
	DailyChatMessages  int
}

func ReadPlanInfo(path string) (PlanInfo, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		if errors.Is(err, fs.ErrNotExist) {
			return PlanInfo{Plan: PlanFree}, nil
		}
		return PlanInfo{}, err
	}
	var info PlanInfo
	if err := json.Unmarshal(data, &info); err != nil {
		return PlanInfo{}, err
	}
	if info.Plan == "" {
		info.Plan = PlanFree
	}
	return info, nil
}

// PlanCaps returns approximate token caps per plan tier.
// These are conservative estimates; refine as Anthropic publishes exact numbers.
func PlanCaps(p Plan) Caps {
	switch p {
	case PlanMax20x:
		return Caps{SessionBlockTokens: 3_000_000, WeeklyAllModels: 40_000_000, WeeklyOpusOnly: 8_000_000, DailyChatMessages: 800}
	case PlanMax5x:
		return Caps{SessionBlockTokens: 1_000_000, WeeklyAllModels: 10_000_000, WeeklyOpusOnly: 2_000_000, DailyChatMessages: 400}
	case PlanPro:
		return Caps{SessionBlockTokens: 200_000, WeeklyAllModels: 2_000_000, WeeklyOpusOnly: 0, DailyChatMessages: 200}
	default:
		return Caps{SessionBlockTokens: 50_000, WeeklyAllModels: 500_000, WeeklyOpusOnly: 0, DailyChatMessages: 50}
	}
}
```

- [ ] **Step 5: Run tests**

Run: `cd daemon && go test ./internal/claudedata/...`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add daemon/internal/claudedata/planinfo.go daemon/internal/claudedata/planinfo_test.go daemon/testdata/claude/.claude.json
git commit -m "read plan info and expose plan caps"
```

---

### Task 5: Pricing table + cost estimator

**Files:**
- Create: `daemon/internal/claudedata/pricing.go`
- Create: `daemon/internal/claudedata/pricing_test.go`

- [ ] **Step 1: Write failing test**

```go
// daemon/internal/claudedata/pricing_test.go
package claudedata

import (
	"math"
	"testing"
)

func near(a, b float64) bool { return math.Abs(a-b) < 0.0001 }

func TestEstimate_SonnetInputOutput(t *testing.T) {
	// Sonnet 4.6: $3/M input, $15/M output
	// 1M input + 100k output = 3 + 1.5 = $4.50
	cost := Estimate("claude-sonnet-4-6", Usage{Input: 1_000_000, Output: 100_000})
	if !near(cost, 4.50) {
		t.Errorf("Sonnet cost = %f, want ~4.50", cost)
	}
}

func TestEstimate_OpusWithCache(t *testing.T) {
	// Opus 4.7: $15/M input, $75/M output, $1.50/M cache read, $18.75/M cache creation
	// 100k input + 50k output + 1M cache read + 100k cache creation
	// = 1.5 + 3.75 + 1.5 + 1.875 = 8.625
	cost := Estimate("claude-opus-4-7", Usage{
		Input: 100_000, Output: 50_000,
		CacheRead: 1_000_000, CacheCreation: 100_000,
	})
	if !near(cost, 8.625) {
		t.Errorf("Opus cost = %f, want ~8.625", cost)
	}
}

func TestEstimate_UnknownModelReturnsZero(t *testing.T) {
	cost := Estimate("claude-unknown-99", Usage{Input: 1000, Output: 1000})
	if cost != 0 {
		t.Errorf("unknown model cost = %f, want 0", cost)
	}
}
```

- [ ] **Step 2: Run test, confirm failure**

Run: `cd daemon && go test ./internal/claudedata/...`
Expected: build error — `Estimate` undefined.

- [ ] **Step 3: Implement pricing**

```go
// daemon/internal/claudedata/pricing.go
package claudedata

// ModelPrice is dollars per 1M tokens.
type ModelPrice struct {
	InputPer1M         float64
	OutputPer1M        float64
	CacheReadPer1M     float64
	CacheCreationPer1M float64
}

// Pricing is a snapshot of Anthropic's published prices.
// Update when prices change; the version field in /v1/stats can signal staleness.
var Pricing = map[string]ModelPrice{
	"claude-opus-4-7":   {InputPer1M: 15.00, OutputPer1M: 75.00, CacheReadPer1M: 1.50, CacheCreationPer1M: 18.75},
	"claude-sonnet-4-6": {InputPer1M: 3.00, OutputPer1M: 15.00, CacheReadPer1M: 0.30, CacheCreationPer1M: 3.75},
	"claude-haiku-4-5":  {InputPer1M: 1.00, OutputPer1M: 5.00, CacheReadPer1M: 0.10, CacheCreationPer1M: 1.25},
}

func Estimate(model string, u Usage) float64 {
	p, ok := Pricing[model]
	if !ok {
		return 0
	}
	per := func(toks int, rate float64) float64 { return float64(toks) * rate / 1_000_000 }
	return per(u.Input, p.InputPer1M) +
		per(u.Output, p.OutputPer1M) +
		per(u.CacheRead, p.CacheReadPer1M) +
		per(u.CacheCreation, p.CacheCreationPer1M)
}
```

- [ ] **Step 4: Run tests**

Run: `cd daemon && go test ./internal/claudedata/...`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add daemon/internal/claudedata/pricing.go daemon/internal/claudedata/pricing_test.go
git commit -m "embed pricing table and cost estimator"
```

---

### Task 6: Weekly window + budget math

**Files:**
- Create: `daemon/internal/claudedata/budgets.go`
- Create: `daemon/internal/claudedata/budgets_test.go`

- [ ] **Step 1: Write failing test**

```go
// daemon/internal/claudedata/budgets_test.go
package claudedata

import (
	"testing"
	"time"
)

func atTime(s string) time.Time {
	// 2026-05-13 is a Wednesday in UTC; the local zone here is UTC for determinism.
	t, _ := time.Parse(time.RFC3339, s)
	return t
}

func TestWeeklyWindow_MidWeekReturnsPriorMonday(t *testing.T) {
	// Wednesday 14:00 UTC → window starts Mon 09:00 UTC
	now := atTime("2026-05-13T14:00:00Z")
	start, end := WeeklyWindow(now)
	wantStart := atTime("2026-05-11T09:00:00Z")
	wantEnd := atTime("2026-05-18T09:00:00Z")
	if !start.Equal(wantStart) {
		t.Errorf("start = %v, want %v", start, wantStart)
	}
	if !end.Equal(wantEnd) {
		t.Errorf("end = %v, want %v", end, wantEnd)
	}
}

func TestWeeklyWindow_MondayBeforeNineReturnsPriorWeek(t *testing.T) {
	// Mon 08:00 → window starts previous Mon 09:00
	now := atTime("2026-05-11T08:00:00Z")
	start, _ := WeeklyWindow(now)
	want := atTime("2026-05-04T09:00:00Z")
	if !start.Equal(want) {
		t.Errorf("start = %v, want %v", start, want)
	}
}

func TestWeeklyWindow_MondayAfterNineReturnsToday(t *testing.T) {
	now := atTime("2026-05-11T10:00:00Z")
	start, _ := WeeklyWindow(now)
	want := atTime("2026-05-11T09:00:00Z")
	if !start.Equal(want) {
		t.Errorf("start = %v, want %v", start, want)
	}
}

func TestWeeklyUsage_SumsWithinWindow(t *testing.T) {
	now := atTime("2026-05-13T14:00:00Z") // Wed
	recs := []Record{
		// Before window — Sun 23:59 → ignored
		mkRec("2026-05-10T23:59:00Z", "claude-sonnet-4-6", 1000, 2000),
		// In window — Mon 10:00
		mkRec("2026-05-11T10:00:00Z", "claude-sonnet-4-6", 1000, 2000),
		mkRec("2026-05-12T10:00:00Z", "claude-opus-4-7", 500, 1000),
	}
	got := WeeklyUsage(recs, now)
	if got["claude-sonnet-4-6"] != 3000 {
		t.Errorf("sonnet weekly = %d, want 3000", got["claude-sonnet-4-6"])
	}
	if got["claude-opus-4-7"] != 1500 {
		t.Errorf("opus weekly = %d, want 1500", got["claude-opus-4-7"])
	}
}
```

- [ ] **Step 2: Run test, confirm failure**

Run: `cd daemon && go test ./internal/claudedata/...`
Expected: build error — `WeeklyWindow`, `WeeklyUsage` undefined.

- [ ] **Step 3: Implement weekly math**

```go
// daemon/internal/claudedata/budgets.go
package claudedata

import "time"

// WeeklyWindow returns [start, end) for the weekly budget window.
// Reset rule: Monday at 09:00 local time. Anchored to now.Location().
func WeeklyWindow(now time.Time) (time.Time, time.Time) {
	loc := now.Location()
	daysSinceMonday := (int(now.Weekday()) - int(time.Monday) + 7) % 7
	candidate := time.Date(now.Year(), now.Month(), now.Day()-daysSinceMonday, 9, 0, 0, 0, loc)
	if candidate.After(now) {
		candidate = candidate.AddDate(0, 0, -7)
	}
	return candidate, candidate.AddDate(0, 0, 7)
}

// WeeklyUsage returns total tokens per model within the current weekly window.
func WeeklyUsage(records []Record, now time.Time) map[string]int {
	start, end := WeeklyWindow(now)
	out := map[string]int{}
	for _, r := range records {
		if r.Timestamp.Before(start) || !r.Timestamp.Before(end) {
			continue
		}
		out[r.Model] += r.Tokens.Total()
	}
	return out
}
```

- [ ] **Step 4: Run tests**

Run: `cd daemon && go test ./internal/claudedata/...`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add daemon/internal/claudedata/budgets.go daemon/internal/claudedata/budgets_test.go
git commit -m "compute weekly budget window and per-model usage"
```

---

### Task 7: Today's per-model totals

**Files:**
- Create: `daemon/internal/claudedata/models.go`
- Create: `daemon/internal/claudedata/models_test.go`

- [ ] **Step 1: Write failing test**

```go
// daemon/internal/claudedata/models_test.go
package claudedata

import (
	"testing"
)

func TestTodayUsage_OnlyIncludesToday(t *testing.T) {
	now := atTime("2026-05-13T14:00:00Z")
	recs := []Record{
		mkRec("2026-05-12T23:59:00Z", "claude-sonnet-4-6", 1000, 2000), // yesterday
		mkRec("2026-05-13T00:01:00Z", "claude-sonnet-4-6", 100, 200),   // today
		mkRec("2026-05-13T10:00:00Z", "claude-opus-4-7", 50, 100),      // today
	}
	got := TodayUsage(recs, now)
	if got["claude-sonnet-4-6"] != 300 {
		t.Errorf("sonnet today = %d, want 300", got["claude-sonnet-4-6"])
	}
	if got["claude-opus-4-7"] != 150 {
		t.Errorf("opus today = %d, want 150", got["claude-opus-4-7"])
	}
}

func TestTotal_SumsAllModels(t *testing.T) {
	totals := map[string]int{
		"claude-sonnet-4-6": 1000,
		"claude-opus-4-7":   500,
	}
	if Total(totals) != 1500 {
		t.Errorf("Total = %d, want 1500", Total(totals))
	}
}
```

- [ ] **Step 2: Run test, confirm failure**

Run: `cd daemon && go test ./internal/claudedata/...`
Expected: build error — `TodayUsage`, `Total` undefined.

- [ ] **Step 3: Implement**

```go
// daemon/internal/claudedata/models.go
package claudedata

import "time"

// TodayUsage returns per-model token totals for messages with timestamps on
// the same calendar day as now (in now's location).
func TodayUsage(records []Record, now time.Time) map[string]int {
	loc := now.Location()
	dayStart := time.Date(now.Year(), now.Month(), now.Day(), 0, 0, 0, 0, loc)
	dayEnd := dayStart.AddDate(0, 0, 1)
	out := map[string]int{}
	for _, r := range records {
		if r.Timestamp.Before(dayStart) || !r.Timestamp.Before(dayEnd) {
			continue
		}
		out[r.Model] += r.Tokens.Total()
	}
	return out
}

func Total(m map[string]int) int {
	s := 0
	for _, v := range m {
		s += v
	}
	return s
}
```

- [ ] **Step 4: Run tests**

Run: `cd daemon && go test ./internal/claudedata/...`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add daemon/internal/claudedata/models.go daemon/internal/claudedata/models_test.go
git commit -m "compute today's per-model token totals"
```

---

### Task 8: Routines fetcher (shell-out + cache)

**Files:**
- Create: `daemon/internal/routines/routines.go`
- Create: `daemon/internal/routines/routines_test.go`
- Create: `daemon/testdata/routines/list-output.json`

- [ ] **Step 1: Create fixture output**

```json
[
  {"name":"babysit-prs","schedule":"0 */5 * * *","last_run":"2026-05-13T12:00:00-07:00","last_status":"ok","next_run":"2026-05-13T17:00:00-07:00"},
  {"name":"morning-standup","schedule":"0 9 * * *","last_run":"2026-05-13T09:00:00-07:00","last_status":"ok","next_run":"2026-05-14T09:00:00-07:00"},
  {"name":"deps-audit","schedule":"*/30 * * * *","last_run":"2026-05-13T14:18:00-07:00","last_status":"slow","next_run":"2026-05-13T14:48:00-07:00"},
  {"name":"nightly-test-run","schedule":"0 2 * * *","last_run":"2026-05-13T02:00:00-07:00","last_status":"fail","next_run":"2026-05-14T02:00:00-07:00"},
  {"name":"weekly-roundup","schedule":"0 17 * * 5","last_run":null,"last_status":"queued","next_run":"2026-05-15T17:00:00-07:00"}
]
```

Save to `daemon/testdata/routines/list-output.json`.

- [ ] **Step 2: Write failing test**

```go
// daemon/internal/routines/routines_test.go
package routines

import (
	"context"
	"os"
	"testing"
	"time"
)

func TestParse_ReadsFixture(t *testing.T) {
	data, err := os.ReadFile("../../testdata/routines/list-output.json")
	if err != nil {
		t.Fatal(err)
	}
	routines, err := Parse(data)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	if len(routines) != 5 {
		t.Fatalf("got %d routines, want 5", len(routines))
	}
	if routines[0].Name != "babysit-prs" {
		t.Errorf("routines[0].Name = %q, want babysit-prs", routines[0].Name)
	}
	if routines[0].LastStatus != "ok" {
		t.Errorf("routines[0].LastStatus = %q, want ok", routines[0].LastStatus)
	}
	if routines[4].LastRun != nil {
		t.Errorf("routines[4].LastRun should be nil (never ran), got %v", routines[4].LastRun)
	}
}

func TestFetcher_CachesWithinTTL(t *testing.T) {
	calls := 0
	f := &Fetcher{
		TTL: 1 * time.Hour,
		Now: func() time.Time { return time.Unix(1000, 0) },
		exec: func(ctx context.Context) ([]byte, error) {
			calls++
			return []byte("[]"), nil
		},
	}
	_, _ = f.Get(context.Background())
	_, _ = f.Get(context.Background())
	if calls != 1 {
		t.Errorf("exec called %d times, want 1 (second call should be cached)", calls)
	}
}

func TestFetcher_RefetchesAfterTTL(t *testing.T) {
	calls := 0
	now := time.Unix(1000, 0)
	f := &Fetcher{
		TTL: 60 * time.Second,
		Now: func() time.Time { return now },
		exec: func(ctx context.Context) ([]byte, error) {
			calls++
			return []byte("[]"), nil
		},
	}
	_, _ = f.Get(context.Background())
	now = now.Add(61 * time.Second)
	_, _ = f.Get(context.Background())
	if calls != 2 {
		t.Errorf("exec called %d times, want 2 (cache should have expired)", calls)
	}
}
```

- [ ] **Step 3: Run test, confirm failure**

Run: `cd daemon && go test ./internal/routines/...`
Expected: build error — `Parse`, `Fetcher`, `Routine` undefined.

- [ ] **Step 4: Implement**

```go
// daemon/internal/routines/routines.go
package routines

import (
	"context"
	"encoding/json"
	"os/exec"
	"sync"
	"time"
)

type Routine struct {
	Name       string     `json:"name"`
	Schedule   string     `json:"schedule"`
	LastRun    *time.Time `json:"last_run"`
	LastStatus string     `json:"last_status"`
	NextRun    *time.Time `json:"next_run"`
}

func Parse(data []byte) ([]Routine, error) {
	var out []Routine
	if err := json.Unmarshal(data, &out); err != nil {
		return nil, err
	}
	return out, nil
}

// execFunc is the shape we mock in tests.
type execFunc func(ctx context.Context) ([]byte, error)

// Fetcher caches the result of `claude routines list --json` for TTL.
type Fetcher struct {
	TTL time.Duration
	Now func() time.Time
	exec execFunc

	mu        sync.Mutex
	cached    []Routine
	cachedAt  time.Time
	cachedErr error
}

// NewFetcher returns a Fetcher that shells out to the `claude` CLI.
func NewFetcher(ttl time.Duration) *Fetcher {
	return &Fetcher{
		TTL: ttl,
		Now: time.Now,
		exec: func(ctx context.Context) ([]byte, error) {
			cmd := exec.CommandContext(ctx, "claude", "routines", "list", "--json")
			return cmd.Output()
		},
	}
}

func (f *Fetcher) Get(ctx context.Context) ([]Routine, error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	if !f.cachedAt.IsZero() && f.Now().Sub(f.cachedAt) < f.TTL {
		return f.cached, f.cachedErr
	}
	data, err := f.exec(ctx)
	if err != nil {
		f.cached, f.cachedErr, f.cachedAt = nil, err, f.Now()
		return nil, err
	}
	parsed, perr := Parse(data)
	f.cached, f.cachedErr, f.cachedAt = parsed, perr, f.Now()
	return parsed, perr
}
```

- [ ] **Step 5: Run tests**

Run: `cd daemon && go test ./internal/routines/...`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add daemon/internal/routines/ daemon/testdata/routines/
git commit -m "fetch and cache Claude routines via CLI shell-out"
```

---

### Task 9: Stats response types + aggregator

**Files:**
- Create: `daemon/internal/stats/types.go`
- Create: `daemon/internal/stats/aggregator.go`
- Create: `daemon/internal/stats/aggregator_test.go`

- [ ] **Step 1: Define response types**

```go
// daemon/internal/stats/types.go
package stats

import "time"

const SchemaVersion = 1

// Stats is the single response the CYD fetches. Field names mirror the
// design spec's example JSON exactly — do not rename without bumping
// SchemaVersion and the firmware's parser.
type Stats struct {
	Schema      int          `json:"schema"`
	GeneratedAt time.Time    `json:"generated_at"`
	Session     Session      `json:"session"`
	ModelsToday ModelsToday  `json:"models_today"`
	Sonnet      SonnetWeekly `json:"sonnet"`
	Chat        Chat         `json:"chat"`
	Routines    []Routine    `json:"routines"`
	Budgets     Budgets      `json:"budgets"`
}

type Session struct {
	PctUsed          int             `json:"pct_used"`
	MinutesRemaining int             `json:"minutes_remaining"`
	ResetsAt         string          `json:"resets_at"` // HH:MM local
	Models           []ModelTokenRow `json:"models"`
}

type ModelTokenRow struct {
	Model  string `json:"model"`
	Tokens int    `json:"tokens"`
}

type ModelsToday struct {
	TotalTokens int             `json:"total_tokens"`
	ByModel     []ModelTokenRow `json:"by_model"`
	EstCostUSD  float64         `json:"est_cost_usd"`
}

type SonnetWeekly struct {
	WeeklyPct int    `json:"weekly_pct"`
	Used      int    `json:"used"`
	Cap       int    `json:"cap"`
	Pace      string `json:"pace"` // "behind" | "on_track" | "ahead"
}

type Chat struct {
	MessagesToday int    `json:"messages_today"`
	DailyCap      int    `json:"daily_cap"`
	ResetsAt      string `json:"resets_at"`
}

type Routine struct {
	Name       string `json:"name"`
	Status     string `json:"status"`
	LastRun    string `json:"last_run"` // HH:MM or "—"
	NextRun    string `json:"next_run"`
}

type Budgets struct {
	CodeAllPct  int    `json:"code_all"`
	CodeOpusPct int    `json:"code_opus"`
	ChatPct     int    `json:"chat"`
	Plan        string `json:"plan"`
	ResetsIn    string `json:"resets_in"` // e.g., "3d19h"
}
```

- [ ] **Step 2: Write failing test for aggregator**

```go
// daemon/internal/stats/aggregator_test.go
package stats

import (
	"context"
	"testing"
	"time"

	"github.com/krizdingus/cydmonitor/daemon/internal/claudedata"
	"github.com/krizdingus/cydmonitor/daemon/internal/routines"
)

func atTime(s string) time.Time {
	t, _ := time.Parse(time.RFC3339, s)
	return t
}

type fakeRoutines struct{ list []routines.Routine }

func (f *fakeRoutines) Get(ctx context.Context) ([]routines.Routine, error) {
	return f.list, nil
}

func TestAggregate_BuildsSessionFromActiveBlock(t *testing.T) {
	now := atTime("2026-05-13T13:00:00Z")
	records := []claudedata.Record{
		{Timestamp: atTime("2026-05-13T10:00:00Z"), Model: "claude-sonnet-4-6", Tokens: claudedata.Usage{Input: 50_000, Output: 50_000}},
		{Timestamp: atTime("2026-05-13T11:00:00Z"), Model: "claude-opus-4-7", Tokens: claudedata.Usage{Input: 20_000, Output: 20_000}},
	}
	agg := &Aggregator{
		Records:   records,
		PlanInfo:  claudedata.PlanInfo{Plan: claudedata.PlanMax5x},
		Routines:  &fakeRoutines{},
		Now:       func() time.Time { return now },
	}
	got, err := agg.Build(context.Background())
	if err != nil {
		t.Fatal(err)
	}
	if got.Schema != SchemaVersion {
		t.Errorf("Schema = %d, want %d", got.Schema, SchemaVersion)
	}
	if got.Session.MinutesRemaining != 120 {
		t.Errorf("MinutesRemaining = %d, want 120", got.Session.MinutesRemaining)
	}
	if len(got.Session.Models) == 0 {
		t.Errorf("expected Session.Models populated")
	}
}

func TestAggregate_NoRecordsReturnsEmptySession(t *testing.T) {
	now := atTime("2026-05-13T13:00:00Z")
	agg := &Aggregator{
		Records:  nil,
		PlanInfo: claudedata.PlanInfo{Plan: claudedata.PlanFree},
		Routines: &fakeRoutines{},
		Now:      func() time.Time { return now },
	}
	got, err := agg.Build(context.Background())
	if err != nil {
		t.Fatal(err)
	}
	if got.Session.PctUsed != 0 {
		t.Errorf("PctUsed = %d, want 0 when no records", got.Session.PctUsed)
	}
	if got.ModelsToday.TotalTokens != 0 {
		t.Errorf("TotalTokens = %d, want 0", got.ModelsToday.TotalTokens)
	}
}

func TestAggregate_PopulatesRoutines(t *testing.T) {
	now := atTime("2026-05-13T13:00:00Z")
	lastRun := atTime("2026-05-13T12:00:00Z")
	rs := &fakeRoutines{
		list: []routines.Routine{
			{Name: "babysit-prs", LastStatus: "ok", LastRun: &lastRun},
		},
	}
	agg := &Aggregator{
		Records:  nil,
		PlanInfo: claudedata.PlanInfo{Plan: claudedata.PlanPro},
		Routines: rs,
		Now:      func() time.Time { return now },
	}
	got, _ := agg.Build(context.Background())
	if len(got.Routines) != 1 {
		t.Fatalf("got %d routines, want 1", len(got.Routines))
	}
	if got.Routines[0].Status != "ok" {
		t.Errorf("Status = %q, want ok", got.Routines[0].Status)
	}
}
```

- [ ] **Step 3: Run, confirm failure**

Run: `cd daemon && go test ./internal/stats/...`
Expected: build error — `Aggregator` undefined.

- [ ] **Step 4: Implement aggregator**

```go
// daemon/internal/stats/aggregator.go
package stats

import (
	"context"
	"fmt"
	"sort"
	"time"

	"github.com/krizdingus/cydmonitor/daemon/internal/claudedata"
	"github.com/krizdingus/cydmonitor/daemon/internal/routines"
)

type routinesSource interface {
	Get(ctx context.Context) ([]routines.Routine, error)
}

type Aggregator struct {
	Records  []claudedata.Record
	PlanInfo claudedata.PlanInfo
	Routines routinesSource
	Now      func() time.Time
}

func (a *Aggregator) Build(ctx context.Context) (Stats, error) {
	now := a.Now()
	caps := claudedata.PlanCaps(a.PlanInfo.Plan)
	blocks := claudedata.ComputeBlocks(a.Records, 5*time.Hour)
	active := claudedata.ActiveBlock(blocks, now)

	out := Stats{
		Schema:      SchemaVersion,
		GeneratedAt: now,
		Session:     buildSession(active, caps, now),
		ModelsToday: buildModelsToday(a.Records, now),
		Sonnet:      buildSonnet(a.Records, caps, now),
		Chat:        Chat{MessagesToday: 0, DailyCap: caps.DailyChatMessages, ResetsAt: "00:00"},
		Budgets:     buildBudgets(a.Records, a.PlanInfo.Plan, caps, now),
	}

	rs, err := a.Routines.Get(ctx)
	if err == nil {
		out.Routines = convertRoutines(rs)
	}
	return out, nil
}

func buildSession(b *claudedata.Block, caps claudedata.Caps, now time.Time) Session {
	if b == nil || caps.SessionBlockTokens == 0 {
		return Session{}
	}
	pct := b.TokensUsed * 100 / caps.SessionBlockTokens
	if pct > 100 {
		pct = 100
	}
	type modelRow struct{ Model string; Tokens int }
	rows := make([]modelRow, 0, len(b.PerModel))
	for m, t := range b.PerModel {
		rows = append(rows, modelRow{m, t})
	}
	sort.Slice(rows, func(i, j int) bool { return rows[i].Tokens > rows[j].Tokens })
	top := []ModelTokenRow{}
	for i, r := range rows {
		if i >= 3 {
			break
		}
		top = append(top, ModelTokenRow{Model: r.Model, Tokens: r.Tokens})
	}
	return Session{
		PctUsed:          pct,
		MinutesRemaining: int(b.End.Sub(now).Minutes()),
		ResetsAt:         b.End.Format("15:04"),
		Models:           top,
	}
}

func buildModelsToday(recs []claudedata.Record, now time.Time) ModelsToday {
	totals := claudedata.TodayUsage(recs, now)
	cost := 0.0
	for _, r := range recs {
		y, m, d := now.Date()
		ry, rm, rd := r.Timestamp.In(now.Location()).Date()
		if y == ry && m == rm && d == rd {
			cost += claudedata.Estimate(r.Model, r.Tokens)
		}
	}
	rows := []ModelTokenRow{}
	for m, t := range totals {
		rows = append(rows, ModelTokenRow{Model: m, Tokens: t})
	}
	sort.Slice(rows, func(i, j int) bool { return rows[i].Tokens > rows[j].Tokens })
	return ModelsToday{TotalTokens: claudedata.Total(totals), ByModel: rows, EstCostUSD: cost}
}

func buildSonnet(recs []claudedata.Record, caps claudedata.Caps, now time.Time) SonnetWeekly {
	weekly := claudedata.WeeklyUsage(recs, now)
	used := 0
	for model, t := range weekly {
		if model == "claude-sonnet-4-6" {
			used += t
		}
	}
	cap := caps.WeeklyAllModels // overall cap for Sonnet on Pro/Max
	pct := 0
	if cap > 0 {
		pct = used * 100 / cap
		if pct > 100 {
			pct = 100
		}
	}
	return SonnetWeekly{WeeklyPct: pct, Used: used, Cap: cap, Pace: "on_track"}
}

func buildBudgets(recs []claudedata.Record, plan claudedata.Plan, caps claudedata.Caps, now time.Time) Budgets {
	weekly := claudedata.WeeklyUsage(recs, now)
	allTotal := claudedata.Total(weekly)
	opus := weekly["claude-opus-4-7"]
	codeAll := 0
	if caps.WeeklyAllModels > 0 {
		codeAll = allTotal * 100 / caps.WeeklyAllModels
		if codeAll > 100 {
			codeAll = 100
		}
	}
	codeOpus := 0
	if caps.WeeklyOpusOnly > 0 {
		codeOpus = opus * 100 / caps.WeeklyOpusOnly
		if codeOpus > 100 {
			codeOpus = 100
		}
	}
	_, end := claudedata.WeeklyWindow(now)
	remaining := end.Sub(now)
	return Budgets{
		CodeAllPct:  codeAll,
		CodeOpusPct: codeOpus,
		ChatPct:     0,
		Plan:        string(plan),
		ResetsIn:    formatDuration(remaining),
	}
}

func convertRoutines(rs []routines.Routine) []Routine {
	out := []Routine{}
	for _, r := range rs {
		last := "—"
		if r.LastRun != nil {
			last = r.LastRun.Local().Format("15:04")
		}
		next := "—"
		if r.NextRun != nil {
			next = r.NextRun.Local().Format("15:04")
		}
		out = append(out, Routine{Name: r.Name, Status: r.LastStatus, LastRun: last, NextRun: next})
	}
	return out
}

func formatDuration(d time.Duration) string {
	if d < 0 {
		return "0h"
	}
	days := int(d.Hours()) / 24
	hours := int(d.Hours()) % 24
	if days > 0 {
		return fmt.Sprintf("%dd%dh", days, hours)
	}
	return fmt.Sprintf("%dh", hours)
}
```

- [ ] **Step 5: Run tests**

Run: `cd daemon && go test ./internal/stats/...`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add daemon/internal/stats/
git commit -m "aggregate parsed Claude data into Stats response"
```

---

### Task 10: Pairings token store

**Files:**
- Create: `daemon/internal/pairings/store.go`
- Create: `daemon/internal/pairings/store_test.go`

- [ ] **Step 1: Write failing test**

```go
// daemon/internal/pairings/store_test.go
package pairings

import (
	"path/filepath"
	"testing"
)

func TestStore_AddAndLookup(t *testing.T) {
	dir := t.TempDir()
	s, err := NewStore(filepath.Join(dir, "pairings.json"))
	if err != nil {
		t.Fatal(err)
	}
	tok, err := s.Add("cyd-A1B2", "Living Room CYD")
	if err != nil {
		t.Fatal(err)
	}
	if len(tok) < 32 {
		t.Errorf("token too short: %d chars", len(tok))
	}
	pair, ok := s.Lookup(tok)
	if !ok {
		t.Fatal("Lookup failed for issued token")
	}
	if pair.CydID != "cyd-A1B2" {
		t.Errorf("CydID = %q, want cyd-A1B2", pair.CydID)
	}
}

func TestStore_PersistsAcrossInstances(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "pairings.json")

	s1, _ := NewStore(path)
	tok, _ := s1.Add("cyd-1", "Test")

	s2, err := NewStore(path)
	if err != nil {
		t.Fatal(err)
	}
	if _, ok := s2.Lookup(tok); !ok {
		t.Errorf("token not persisted across NewStore calls")
	}
}

func TestStore_Reset(t *testing.T) {
	dir := t.TempDir()
	s, _ := NewStore(filepath.Join(dir, "pairings.json"))
	tok, _ := s.Add("cyd-1", "Test")
	if err := s.Reset(); err != nil {
		t.Fatal(err)
	}
	if _, ok := s.Lookup(tok); ok {
		t.Errorf("Reset should have cleared tokens")
	}
}

func TestStore_List(t *testing.T) {
	dir := t.TempDir()
	s, _ := NewStore(filepath.Join(dir, "pairings.json"))
	_, _ = s.Add("a", "A")
	_, _ = s.Add("b", "B")
	pairs := s.List()
	if len(pairs) != 2 {
		t.Errorf("List returned %d, want 2", len(pairs))
	}
}
```

- [ ] **Step 2: Run test, confirm failure**

Run: `cd daemon && go test ./internal/pairings/...`
Expected: build error.

- [ ] **Step 3: Implement store**

```go
// daemon/internal/pairings/store.go
package pairings

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"errors"
	"io/fs"
	"os"
	"path/filepath"
	"sync"
	"time"
)

type Pair struct {
	Token     string    `json:"token"`
	CydID     string    `json:"cyd_id"`
	Name      string    `json:"name"`
	CreatedAt time.Time `json:"created_at"`
}

type Store struct {
	path string
	mu   sync.Mutex
	pairs map[string]Pair // token → pair
}

func NewStore(path string) (*Store, error) {
	s := &Store{path: path, pairs: map[string]Pair{}}
	if err := s.load(); err != nil {
		return nil, err
	}
	return s, nil
}

func (s *Store) load() error {
	data, err := os.ReadFile(s.path)
	if errors.Is(err, fs.ErrNotExist) {
		return nil
	}
	if err != nil {
		return err
	}
	var list []Pair
	if err := json.Unmarshal(data, &list); err != nil {
		return err
	}
	for _, p := range list {
		s.pairs[p.Token] = p
	}
	return nil
}

func (s *Store) save() error {
	if err := os.MkdirAll(filepath.Dir(s.path), 0o755); err != nil {
		return err
	}
	list := make([]Pair, 0, len(s.pairs))
	for _, p := range s.pairs {
		list = append(list, p)
	}
	data, err := json.MarshalIndent(list, "", "  ")
	if err != nil {
		return err
	}
	return os.WriteFile(s.path, data, 0o600)
}

func (s *Store) Add(cydID, name string) (string, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	buf := make([]byte, 32)
	if _, err := rand.Read(buf); err != nil {
		return "", err
	}
	token := hex.EncodeToString(buf)
	s.pairs[token] = Pair{Token: token, CydID: cydID, Name: name, CreatedAt: time.Now()}
	return token, s.save()
}

func (s *Store) Lookup(token string) (Pair, bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	p, ok := s.pairs[token]
	return p, ok
}

func (s *Store) List() []Pair {
	s.mu.Lock()
	defer s.mu.Unlock()
	out := make([]Pair, 0, len(s.pairs))
	for _, p := range s.pairs {
		out = append(out, p)
	}
	return out
}

func (s *Store) Reset() error {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.pairs = map[string]Pair{}
	return s.save()
}
```

- [ ] **Step 4: Run tests**

Run: `cd daemon && go test ./internal/pairings/...`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add daemon/internal/pairings/
git commit -m "persist paired-client tokens on disk"
```

---

### Task 11: 4-digit pairing code generator

**Files:**
- Create: `daemon/internal/pairings/codes.go`
- Create: `daemon/internal/pairings/codes_test.go`

- [ ] **Step 1: Write failing test**

```go
// daemon/internal/pairings/codes_test.go
package pairings

import (
	"testing"
	"time"
)

func TestCodes_RequestReturnsFourDigitCode(t *testing.T) {
	c := NewCodes(2 * time.Minute)
	req, err := c.Request("cyd-X")
	if err != nil {
		t.Fatal(err)
	}
	if len(req.Code) != 4 {
		t.Errorf("code length = %d, want 4", len(req.Code))
	}
	for _, ch := range req.Code {
		if ch < '0' || ch > '9' {
			t.Errorf("code contains non-digit: %q", req.Code)
		}
	}
}

func TestCodes_VerifyAcceptsCorrectCode(t *testing.T) {
	c := NewCodes(2 * time.Minute)
	req, _ := c.Request("cyd-X")
	cydID, ok := c.Verify(req.Code)
	if !ok {
		t.Fatal("Verify should accept correct code")
	}
	if cydID != "cyd-X" {
		t.Errorf("cydID = %q, want cyd-X", cydID)
	}
}

func TestCodes_VerifyRejectsWrongCode(t *testing.T) {
	c := NewCodes(2 * time.Minute)
	_, _ = c.Request("cyd-X")
	if _, ok := c.Verify("0000"); ok {
		t.Errorf("Verify should reject wrong code")
	}
}

func TestCodes_CodeExpires(t *testing.T) {
	c := NewCodes(1 * time.Second)
	now := time.Unix(1000, 0)
	c.Now = func() time.Time { return now }
	req, _ := c.Request("cyd-X")
	now = now.Add(2 * time.Second)
	if _, ok := c.Verify(req.Code); ok {
		t.Errorf("Verify should reject expired code")
	}
}

func TestCodes_VerifyConsumesCode(t *testing.T) {
	c := NewCodes(2 * time.Minute)
	req, _ := c.Request("cyd-X")
	if _, ok := c.Verify(req.Code); !ok {
		t.Fatal("first verify should succeed")
	}
	if _, ok := c.Verify(req.Code); ok {
		t.Errorf("second verify of same code should fail")
	}
}

func TestCodes_Pending(t *testing.T) {
	c := NewCodes(2 * time.Minute)
	if c.Pending() != "" {
		t.Errorf("Pending should be empty initially")
	}
	req, _ := c.Request("cyd-X")
	if c.Pending() != req.Code {
		t.Errorf("Pending = %q, want %q", c.Pending(), req.Code)
	}
}
```

- [ ] **Step 2: Run, confirm failure**

Run: `cd daemon && go test ./internal/pairings/...`
Expected: build error.

- [ ] **Step 3: Implement codes**

```go
// daemon/internal/pairings/codes.go
package pairings

import (
	"crypto/rand"
	"encoding/binary"
	"fmt"
	"sync"
	"time"
)

type PendingRequest struct {
	CydID     string
	Code      string
	ExpiresAt time.Time
}

type Codes struct {
	TTL time.Duration
	Now func() time.Time

	mu      sync.Mutex
	current *PendingRequest
}

func NewCodes(ttl time.Duration) *Codes {
	return &Codes{TTL: ttl, Now: time.Now}
}

func (c *Codes) Request(cydID string) (PendingRequest, error) {
	c.mu.Lock()
	defer c.mu.Unlock()
	var buf [4]byte
	if _, err := rand.Read(buf[:]); err != nil {
		return PendingRequest{}, err
	}
	n := binary.BigEndian.Uint32(buf[:]) % 10000
	code := fmt.Sprintf("%04d", n)
	c.current = &PendingRequest{
		CydID:     cydID,
		Code:      code,
		ExpiresAt: c.Now().Add(c.TTL),
	}
	return *c.current, nil
}

func (c *Codes) Verify(code string) (string, bool) {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.current == nil {
		return "", false
	}
	if c.Now().After(c.current.ExpiresAt) {
		c.current = nil
		return "", false
	}
	if c.current.Code != code {
		return "", false
	}
	cydID := c.current.CydID
	c.current = nil
	return cydID, true
}

func (c *Codes) Pending() string {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.current == nil || c.Now().After(c.current.ExpiresAt) {
		return ""
	}
	return c.current.Code
}
```

- [ ] **Step 4: Run tests**

Run: `cd daemon && go test ./internal/pairings/...`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add daemon/internal/pairings/codes.go daemon/internal/pairings/codes_test.go
git commit -m "generate 4-digit pairing codes with TTL"
```

---

### Task 12: HTTP auth middleware

**Files:**
- Create: `daemon/internal/server/auth.go`
- Create: `daemon/internal/server/auth_test.go`

- [ ] **Step 1: Write failing test**

```go
// daemon/internal/server/auth_test.go
package server

import (
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"testing"

	"github.com/krizdingus/cydmonitor/daemon/internal/pairings"
)

func TestAuth_RejectsMissingHeader(t *testing.T) {
	store, _ := pairings.NewStore(filepath.Join(t.TempDir(), "p.json"))
	h := RequireToken(store, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(200)
	}))
	rec := httptest.NewRecorder()
	req := httptest.NewRequest("GET", "/", nil)
	h.ServeHTTP(rec, req)
	if rec.Code != http.StatusUnauthorized {
		t.Errorf("status = %d, want 401", rec.Code)
	}
}

func TestAuth_RejectsBadToken(t *testing.T) {
	store, _ := pairings.NewStore(filepath.Join(t.TempDir(), "p.json"))
	h := RequireToken(store, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(200)
	}))
	rec := httptest.NewRecorder()
	req := httptest.NewRequest("GET", "/", nil)
	req.Header.Set("Authorization", "Bearer not-a-real-token")
	h.ServeHTTP(rec, req)
	if rec.Code != http.StatusUnauthorized {
		t.Errorf("status = %d, want 401", rec.Code)
	}
}

func TestAuth_AcceptsValidToken(t *testing.T) {
	store, _ := pairings.NewStore(filepath.Join(t.TempDir(), "p.json"))
	tok, _ := store.Add("cyd-1", "test")
	h := RequireToken(store, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(200)
	}))
	rec := httptest.NewRecorder()
	req := httptest.NewRequest("GET", "/", nil)
	req.Header.Set("Authorization", "Bearer "+tok)
	h.ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Errorf("status = %d, want 200", rec.Code)
	}
}
```

- [ ] **Step 2: Run, confirm failure**

Run: `cd daemon && go test ./internal/server/...`
Expected: build error — `RequireToken` undefined.

- [ ] **Step 3: Implement middleware**

```go
// daemon/internal/server/auth.go
package server

import (
	"net/http"
	"strings"

	"github.com/krizdingus/cydmonitor/daemon/internal/pairings"
)

// RequireToken wraps a handler to enforce a valid Bearer token in the
// Authorization header. Tokens are looked up in the pairings store.
func RequireToken(store *pairings.Store, next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		header := r.Header.Get("Authorization")
		token, ok := strings.CutPrefix(header, "Bearer ")
		if !ok || token == "" {
			http.Error(w, "missing or malformed Authorization header", http.StatusUnauthorized)
			return
		}
		if _, found := store.Lookup(token); !found {
			http.Error(w, "invalid token", http.StatusUnauthorized)
			return
		}
		next.ServeHTTP(w, r)
	})
}
```

- [ ] **Step 4: Run tests**

Run: `cd daemon && go test ./internal/server/...`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add daemon/internal/server/auth.go daemon/internal/server/auth_test.go
git commit -m "bearer token auth middleware"
```

---

### Task 13: HTTP handlers

**Files:**
- Create: `daemon/internal/server/handlers.go`
- Create: `daemon/internal/server/handlers_test.go`

- [ ] **Step 1: Write failing test**

```go
// daemon/internal/server/handlers_test.go
package server

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"github.com/krizdingus/cydmonitor/daemon/internal/claudedata"
	"github.com/krizdingus/cydmonitor/daemon/internal/pairings"
	"github.com/krizdingus/cydmonitor/daemon/internal/routines"
	"github.com/krizdingus/cydmonitor/daemon/internal/stats"
)

type fakeRoutines struct{}

func (fakeRoutines) Get(ctx context.Context) ([]routines.Routine, error) { return nil, nil }

func newTestServer(t *testing.T) (*Server, string) {
	t.Helper()
	store, _ := pairings.NewStore(filepath.Join(t.TempDir(), "p.json"))
	tok, _ := store.Add("cyd-1", "test")
	codes := pairings.NewCodes(2 * time.Minute)
	agg := &stats.Aggregator{
		Records:  nil,
		PlanInfo: claudedata.PlanInfo{Plan: claudedata.PlanPro},
		Routines: fakeRoutines{},
		Now:      time.Now,
	}
	s := New(Config{
		Store:      store,
		Codes:      codes,
		Aggregator: agg,
		Version:    "test",
	})
	return s, tok
}

func TestStatsHandler_RequiresAuth(t *testing.T) {
	s, _ := newTestServer(t)
	rec := httptest.NewRecorder()
	req := httptest.NewRequest("GET", "/v1/stats", nil)
	s.Handler().ServeHTTP(rec, req)
	if rec.Code != http.StatusUnauthorized {
		t.Errorf("status = %d, want 401", rec.Code)
	}
}

func TestStatsHandler_ReturnsJSONWithValidToken(t *testing.T) {
	s, tok := newTestServer(t)
	rec := httptest.NewRecorder()
	req := httptest.NewRequest("GET", "/v1/stats", nil)
	req.Header.Set("Authorization", "Bearer "+tok)
	s.Handler().ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, body = %s", rec.Code, rec.Body.String())
	}
	if got := rec.Header().Get("Content-Type"); !strings.HasPrefix(got, "application/json") {
		t.Errorf("Content-Type = %q", got)
	}
	var out stats.Stats
	if err := json.Unmarshal(rec.Body.Bytes(), &out); err != nil {
		t.Fatalf("response not JSON: %v", err)
	}
	if out.Schema != stats.SchemaVersion {
		t.Errorf("Schema = %d, want %d", out.Schema, stats.SchemaVersion)
	}
}

func TestPairInitHandler_ReturnsCode(t *testing.T) {
	s, _ := newTestServer(t)
	rec := httptest.NewRecorder()
	body := strings.NewReader(`{"cyd_id":"cyd-NEW"}`)
	req := httptest.NewRequest("POST", "/v1/pair-init", body)
	s.Handler().ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, body = %s", rec.Code, rec.Body.String())
	}
	var resp struct {
		Code string `json:"code"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &resp); err != nil {
		t.Fatal(err)
	}
	if len(resp.Code) != 4 {
		t.Errorf("code length = %d, want 4", len(resp.Code))
	}
}

func TestPairVerifyHandler_IssuesToken(t *testing.T) {
	s, _ := newTestServer(t)

	// First: init pairing to get a code
	rec1 := httptest.NewRecorder()
	body1 := strings.NewReader(`{"cyd_id":"cyd-NEW"}`)
	req1 := httptest.NewRequest("POST", "/v1/pair-init", body1)
	s.Handler().ServeHTTP(rec1, req1)
	var initResp struct{ Code string `json:"code"` }
	_ = json.Unmarshal(rec1.Body.Bytes(), &initResp)

	// Then: verify
	rec2 := httptest.NewRecorder()
	body2 := strings.NewReader(`{"cyd_id":"cyd-NEW","code":"` + initResp.Code + `","name":"Office"}`)
	req2 := httptest.NewRequest("POST", "/v1/pair-verify", body2)
	s.Handler().ServeHTTP(rec2, req2)
	if rec2.Code != http.StatusOK {
		t.Fatalf("status = %d, body = %s", rec2.Code, rec2.Body.String())
	}
	var verifyResp struct{ Token string `json:"token"` }
	_ = json.Unmarshal(rec2.Body.Bytes(), &verifyResp)
	if len(verifyResp.Token) < 32 {
		t.Errorf("token too short: %d", len(verifyResp.Token))
	}
}

func TestPairVerifyHandler_RejectsBadCode(t *testing.T) {
	s, _ := newTestServer(t)
	rec := httptest.NewRecorder()
	body := strings.NewReader(`{"cyd_id":"cyd-X","code":"0000","name":""}`)
	req := httptest.NewRequest("POST", "/v1/pair-verify", body)
	s.Handler().ServeHTTP(rec, req)
	if rec.Code != http.StatusUnauthorized {
		t.Errorf("status = %d, want 401", rec.Code)
	}
}

func TestStatusHandler_ReturnsState(t *testing.T) {
	s, _ := newTestServer(t)
	rec := httptest.NewRecorder()
	req := httptest.NewRequest("GET", "/v1/status", nil)
	s.Handler().ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d", rec.Code)
	}
	var out struct {
		Version     string `json:"version"`
		PairedCount int    `json:"paired_count"`
		PendingCode string `json:"pending_code"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &out); err != nil {
		t.Fatal(err)
	}
	if out.Version != "test" {
		t.Errorf("Version = %q, want test", out.Version)
	}
	if out.PairedCount != 1 {
		t.Errorf("PairedCount = %d, want 1", out.PairedCount)
	}
}
```

- [ ] **Step 2: Run, confirm failure**

Run: `cd daemon && go test ./internal/server/...`
Expected: build error.

- [ ] **Step 3: Implement handlers + Server type**

```go
// daemon/internal/server/handlers.go
package server

import (
	"context"
	"encoding/json"
	"net/http"

	"github.com/krizdingus/cydmonitor/daemon/internal/pairings"
	"github.com/krizdingus/cydmonitor/daemon/internal/stats"
)

type aggregator interface {
	Build(ctx context.Context) (stats.Stats, error)
}

type Config struct {
	Store      *pairings.Store
	Codes      *pairings.Codes
	Aggregator aggregator
	Version    string
}

type Server struct {
	cfg Config
}

func New(cfg Config) *Server { return &Server{cfg: cfg} }

func (s *Server) Handler() http.Handler {
	mux := http.NewServeMux()

	// Authenticated:
	mux.Handle("GET /v1/stats", RequireToken(s.cfg.Store, http.HandlerFunc(s.stats)))

	// Unauthenticated:
	mux.HandleFunc("POST /v1/pair-init", s.pairInit)
	mux.HandleFunc("POST /v1/pair-verify", s.pairVerify)
	mux.HandleFunc("GET /v1/status", s.status)

	return mux
}

func (s *Server) stats(w http.ResponseWriter, r *http.Request) {
	out, err := s.cfg.Aggregator.Build(r.Context())
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	writeJSON(w, http.StatusOK, out)
}

type pairInitReq struct {
	CydID string `json:"cyd_id"`
}
type pairInitResp struct {
	Code string `json:"code"`
}

func (s *Server) pairInit(w http.ResponseWriter, r *http.Request) {
	var req pairInitReq
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil || req.CydID == "" {
		http.Error(w, "bad request", http.StatusBadRequest)
		return
	}
	pending, err := s.cfg.Codes.Request(req.CydID)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	writeJSON(w, http.StatusOK, pairInitResp{Code: pending.Code})
}

type pairVerifyReq struct {
	CydID string `json:"cyd_id"`
	Code  string `json:"code"`
	Name  string `json:"name"`
}
type pairVerifyResp struct {
	Token string `json:"token"`
}

func (s *Server) pairVerify(w http.ResponseWriter, r *http.Request) {
	var req pairVerifyReq
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, "bad request", http.StatusBadRequest)
		return
	}
	cydID, ok := s.cfg.Codes.Verify(req.Code)
	if !ok || cydID != req.CydID {
		http.Error(w, "invalid code", http.StatusUnauthorized)
		return
	}
	name := req.Name
	if name == "" {
		name = cydID
	}
	tok, err := s.cfg.Store.Add(cydID, name)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	writeJSON(w, http.StatusOK, pairVerifyResp{Token: tok})
}

type statusResp struct {
	Version     string `json:"version"`
	PairedCount int    `json:"paired_count"`
	PendingCode string `json:"pending_code"`
}

func (s *Server) status(w http.ResponseWriter, _ *http.Request) {
	writeJSON(w, http.StatusOK, statusResp{
		Version:     s.cfg.Version,
		PairedCount: len(s.cfg.Store.List()),
		PendingCode: s.cfg.Codes.Pending(),
	})
}

func writeJSON(w http.ResponseWriter, code int, v interface{}) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(code)
	_ = json.NewEncoder(w).Encode(v)
}
```

- [ ] **Step 4: Run tests**

Run: `cd daemon && go test ./internal/server/...`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add daemon/internal/server/handlers.go daemon/internal/server/handlers_test.go
git commit -m "implement stats, pairing, and status HTTP handlers"
```

---

### Task 14: Server wiring + listener

**Files:**
- Create: `daemon/internal/server/server.go`

- [ ] **Step 1: Implement listener**

```go
// daemon/internal/server/server.go
package server

import (
	"context"
	"errors"
	"fmt"
	"net/http"
	"time"
)

// ListenAndServe binds the server's mux to addr (e.g. "0.0.0.0:7842")
// and blocks until ctx is cancelled or the listener fails.
func (s *Server) ListenAndServe(ctx context.Context, addr string) error {
	srv := &http.Server{
		Addr:              addr,
		Handler:           s.Handler(),
		ReadHeaderTimeout: 5 * time.Second,
	}
	errCh := make(chan error, 1)
	go func() {
		fmt.Printf("daemon listening on %s\n", addr)
		errCh <- srv.ListenAndServe()
	}()
	select {
	case <-ctx.Done():
		shutdownCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		return srv.Shutdown(shutdownCtx)
	case err := <-errCh:
		if errors.Is(err, http.ErrServerClosed) {
			return nil
		}
		return err
	}
}
```

- [ ] **Step 2: Verify package compiles**

Run: `cd daemon && go build ./...`
Expected: no errors.

- [ ] **Step 3: Commit**

```bash
git add daemon/internal/server/server.go
git commit -m "wire HTTP server lifecycle"
```

---

### Task 15: mDNS advertiser

**Files:**
- Create: `daemon/internal/discovery/mdns.go`

- [ ] **Step 1: Implement advertiser**

```go
// daemon/internal/discovery/mdns.go
package discovery

import (
	"context"
	"fmt"

	"github.com/grandcat/zeroconf"
)

const ServiceType = "_claudeusage._tcp"
const Domain = "local."

// Advertise registers the daemon on the LAN. Blocks until ctx is cancelled.
func Advertise(ctx context.Context, instanceName, hostname string, port int, version string, schema int) error {
	txt := []string{
		fmt.Sprintf("host=%s", hostname),
		fmt.Sprintf("version=%s", version),
		fmt.Sprintf("schema=%d", schema),
	}
	server, err := zeroconf.Register(instanceName, ServiceType, Domain, port, txt, nil)
	if err != nil {
		return fmt.Errorf("zeroconf register: %w", err)
	}
	defer server.Shutdown()
	<-ctx.Done()
	return nil
}
```

- [ ] **Step 2: Build to verify the zeroconf dependency resolves**

Run:
```bash
cd daemon && go mod tidy && go build ./...
```
Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add daemon/internal/discovery/ daemon/go.mod daemon/go.sum
git commit -m "advertise daemon on LAN via mDNS"
```

---

### Task 16: Wire main + CLI subcommands

**Files:**
- Modify: `daemon/cmd/cydmonitor/main.go`
- Create: `daemon/internal/cli/status.go`
- Create: `daemon/internal/cli/reset.go`

- [ ] **Step 1: Implement CLI commands**

```go
// daemon/internal/cli/status.go
package cli

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
)

func Status(w io.Writer, baseURL string) int {
	resp, err := http.Get(baseURL + "/v1/status")
	if err != nil {
		fmt.Fprintf(os.Stderr, "daemon not reachable at %s: %v\n", baseURL, err)
		return 1
	}
	defer resp.Body.Close()
	var s struct {
		Version     string `json:"version"`
		PairedCount int    `json:"paired_count"`
		PendingCode string `json:"pending_code"`
	}
	if err := json.NewDecoder(resp.Body).Decode(&s); err != nil {
		fmt.Fprintf(os.Stderr, "bad response: %v\n", err)
		return 1
	}
	fmt.Fprintf(w, "✓ Daemon running (version %s)\n", s.Version)
	fmt.Fprintf(w, "  Paired CYDs: %d\n", s.PairedCount)
	if s.PendingCode != "" {
		fmt.Fprintf(w, "  Pairing code: %s\n", s.PendingCode)
		fmt.Fprintf(w, "  Type this code on your CYD to confirm.\n")
	}
	return 0
}
```

```go
// daemon/internal/cli/reset.go
package cli

import (
	"fmt"
	"io"

	"github.com/krizdingus/cydmonitor/daemon/internal/pairings"
)

func ResetPairings(w io.Writer, path string) int {
	store, err := pairings.NewStore(path)
	if err != nil {
		fmt.Fprintln(w, "error opening store:", err)
		return 1
	}
	count := len(store.List())
	if err := store.Reset(); err != nil {
		fmt.Fprintln(w, "reset failed:", err)
		return 1
	}
	fmt.Fprintf(w, "Removed %d paired CYD(s)\n", count)
	return 0
}
```

- [ ] **Step 2: Replace main.go with wiring**

```go
// daemon/cmd/cydmonitor/main.go
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
```

- [ ] **Step 3: Build and run the daemon**

Run:
```bash
cd daemon && make build && ./bin/cydmonitor &
sleep 1
./bin/cydmonitor status
kill %1 2>/dev/null
```
Expected: `✓ Daemon running (version 0.1.0-dev)` with `Paired CYDs: 0`.

- [ ] **Step 4: Commit**

```bash
git add daemon/cmd/cydmonitor/main.go daemon/internal/cli/
git commit -m "wire serve, status, and reset-pairings entry points"
```

---

### Task 17: End-to-end smoke test

**Files:**
- Create: `daemon/cmd/cydmonitor/main_e2e_test.go`

- [ ] **Step 1: Write the smoke test**

```go
// daemon/cmd/cydmonitor/main_e2e_test.go
package main

import (
	"context"
	"encoding/json"
	"io"
	"net/http"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

// TestE2E_PairAndFetchStats runs the full daemon binary, performs the pairing
// handshake, and fetches /v1/stats. Skipped under `go test -short`.
func TestE2E_PairAndFetchStats(t *testing.T) {
	if testing.Short() {
		t.Skip("skipping e2e in -short mode")
	}
	// Build the binary fresh
	bin := filepath.Join(t.TempDir(), "cydmonitor")
	if out, err := exec.Command("go", "build", "-o", bin, ".").CombinedOutput(); err != nil {
		t.Fatalf("build: %v\n%s", err, out)
	}

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	cmd := exec.CommandContext(ctx, bin)
	cmd.Env = append(cmd.Env, "HOME="+t.TempDir())
	if err := cmd.Start(); err != nil {
		t.Fatalf("start: %v", err)
	}
	defer func() { _ = cmd.Process.Kill() }()

	// Wait for server
	waitForReady(t, "http://127.0.0.1:7842/v1/status", 3*time.Second)

	// Init pairing
	initBody := strings.NewReader(`{"cyd_id":"cyd-e2e"}`)
	initResp, err := http.Post("http://127.0.0.1:7842/v1/pair-init", "application/json", initBody)
	if err != nil {
		t.Fatal(err)
	}
	var initOut struct{ Code string `json:"code"` }
	_ = json.NewDecoder(initResp.Body).Decode(&initOut)
	initResp.Body.Close()
	if len(initOut.Code) != 4 {
		t.Fatalf("got code %q", initOut.Code)
	}

	// Verify pairing
	verifyBody := strings.NewReader(`{"cyd_id":"cyd-e2e","code":"` + initOut.Code + `","name":"E2E"}`)
	vResp, err := http.Post("http://127.0.0.1:7842/v1/pair-verify", "application/json", verifyBody)
	if err != nil {
		t.Fatal(err)
	}
	var vOut struct{ Token string `json:"token"` }
	_ = json.NewDecoder(vResp.Body).Decode(&vOut)
	vResp.Body.Close()
	if len(vOut.Token) < 32 {
		t.Fatalf("got token %q", vOut.Token)
	}

	// Fetch stats
	req, _ := http.NewRequest("GET", "http://127.0.0.1:7842/v1/stats", nil)
	req.Header.Set("Authorization", "Bearer "+vOut.Token)
	sResp, err := http.DefaultClient.Do(req)
	if err != nil {
		t.Fatal(err)
	}
	defer sResp.Body.Close()
	if sResp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(sResp.Body)
		t.Fatalf("stats status = %d, body = %s", sResp.StatusCode, body)
	}
	var stats map[string]interface{}
	_ = json.NewDecoder(sResp.Body).Decode(&stats)
	if stats["schema"] == nil {
		t.Errorf("response missing schema field")
	}
}

func waitForReady(t *testing.T, url string, timeout time.Duration) {
	t.Helper()
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		resp, err := http.Get(url)
		if err == nil {
			resp.Body.Close()
			if resp.StatusCode == 200 {
				return
			}
		}
		time.Sleep(50 * time.Millisecond)
	}
	t.Fatalf("server never reached ready at %s", url)
}
```

- [ ] **Step 2: Run the test**

Run: `cd daemon && go test -v ./cmd/cydmonitor/...`
Expected: PASS.

- [ ] **Step 3: Run the full suite**

Run: `cd daemon && make test`
Expected: all tests PASS across all packages.

- [ ] **Step 4: Commit**

```bash
git add daemon/cmd/cydmonitor/main_e2e_test.go
git commit -m "end-to-end pairing + stats smoke test"
```

---

### Task 18: README

**Files:**
- Create: `daemon/README.md`

- [ ] **Step 1: Write a focused README**

```markdown
# cydmonitor daemon

The host-side service for the CYD Claude Usage Monitor.

## Build & Run

```bash
make build
./bin/cydmonitor          # serves on :7842
./bin/cydmonitor status   # show running state + any pending pairing code
./bin/cydmonitor reset-pairings
```

## What it does

- Reads `~/.claude/projects/*.jsonl` and `~/.claude.json`
- Computes 5-hour session blocks, weekly budgets, today's per-model usage
- Serves a single `/v1/stats` JSON endpoint to paired CYD devices
- Advertises itself on the LAN via mDNS (`_claudeusage._tcp.local`)
- Pairs new CYDs via 4-digit codes shown by `cydmonitor status`

## API

| Method | Path | Auth | Purpose |
|---|---|---|---|
| GET | `/v1/stats` | Bearer token | Aggregated usage data for the CYD |
| POST | `/v1/pair-init` | none | CYD requests a pairing code |
| POST | `/v1/pair-verify` | none | CYD submits code + receives token |
| GET | `/v1/status` | none | Health + pending pairing code |

Token storage: `~/.config/cydmonitor/pairings.json` (mode 0600).

See `docs/superpowers/specs/2026-05-13-cyd-claude-usage-monitor-design.md` for the full design.
```

- [ ] **Step 2: Commit**

```bash
git add daemon/README.md
git commit -m "document daemon build, run, and API surface"
```

---

## Self-Review Notes

After writing all tasks, ran a self-check against the spec:

1. **Spec coverage:** Every Phase-1 daemon responsibility from the spec maps to a task — JSONL parsing (2), session blocks (3), plan tier (4), pricing (5), budgets (6), today's models (7), routines (8), stats aggregator (9), pairing (10–11), HTTP + auth (12–14), mDNS (15), CLI (16), e2e (17). The web UI, OTA, and phase 2/3 items are explicitly deferred in the front matter.
2. **Placeholder scan:** No `TODO`, `TBD`, or "add appropriate X" patterns left in the steps. Pricing values are concrete (with a comment noting they should be refreshed when Anthropic updates them — but the code itself is complete).
3. **Type consistency:** `Usage{Input, Output, CacheRead, CacheCreation}` is consistent across parser, pricing, and aggregator. `Stats` field names match the spec's example JSON. `Routine.LastStatus` (input from CLI) becomes `Routine.Status` (output to CYD) — that mapping happens in `convertRoutines`.
4. **Ambiguity check:** Two pricing tables were collapsed into one (`Pricing` map). The pace string for `SonnetWeekly` is hardcoded to `"on_track"` in MVP — refinement deferred to phase 2 (noted implicitly by leaving it constant).
