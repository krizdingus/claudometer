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
