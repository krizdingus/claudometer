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

// Total returns the sum of all values in m.
func Total(m map[string]int) int {
	s := 0
	for _, v := range m {
		s += v
	}
	return s
}
