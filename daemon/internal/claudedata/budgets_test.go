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
