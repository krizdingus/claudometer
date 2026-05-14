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
