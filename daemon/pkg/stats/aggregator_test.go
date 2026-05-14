package stats

import (
	"context"
	"testing"
	"time"

	"github.com/krizdingus/claudometer/daemon/pkg/claudedata"
	"github.com/krizdingus/claudometer/daemon/pkg/routines"
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
		Caps:      claudedata.PlanCaps(claudedata.PlanMax5x),
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
		Caps:     claudedata.PlanCaps(claudedata.PlanFree),
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
		Caps:     claudedata.PlanCaps(claudedata.PlanPro),
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
