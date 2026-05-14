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
