package stats

import "time"

const SchemaVersion = 1

// Stats is the single response the CYD fetches. Field names mirror the
// design spec's example JSON exactly — do not rename without bumping
// SchemaVersion and the firmware's parser.
type Stats struct {
	Schema      int          `json:"schema"`
	GeneratedAt time.Time    `json:"generated_at"`
	LocalTime   string       `json:"local_time"` // HH:MM in the daemon host's local TZ
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
