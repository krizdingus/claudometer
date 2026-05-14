package claudedata

import (
	"encoding/json"
	"errors"
	"io/fs"
	"os"
	"strconv"
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
	// CYDMONITOR_PLAN overrides anything we read from disk. Claude Code does
	// not currently store the plan tier in ~/.claude.json in a way we can
	// reliably detect, so this env var is the supported way to tell the
	// daemon "I'm on Max-5x" / "Max-20x" / etc. Values: free, pro, max-5x,
	// max-20x.
	if override := os.Getenv("CYDMONITOR_PLAN"); override != "" {
		return PlanInfo{Plan: Plan(override)}, nil
	}
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

// PlanCaps returns approximate token caps per plan tier. Anthropic does not
// publish these as exact numbers; the Max-5x values below are reverse-derived
// from live percentages reported by claude.ai's usage page vs our local
// (cache_read-excluded, dedup'd by message id) token counts. Max-20x is scaled
// 4x off that.
//
// Override any cap at runtime with the matching env var:
//   CYDMONITOR_SESSION_TOKENS, CYDMONITOR_WEEKLY_ALL,
//   CYDMONITOR_WEEKLY_OPUS, CYDMONITOR_DAILY_CHAT_MESSAGES.
func PlanCaps(p Plan) Caps {
	var c Caps
	switch p {
	case PlanMax20x:
		c = Caps{SessionBlockTokens: 20_000_000, WeeklyAllModels: 180_000_000, WeeklyOpusOnly: 36_000_000, DailyChatMessages: 800}
	case PlanMax5x:
		c = Caps{SessionBlockTokens: 5_000_000, WeeklyAllModels: 45_000_000, WeeklyOpusOnly: 9_000_000, DailyChatMessages: 400}
	case PlanPro:
		c = Caps{SessionBlockTokens: 1_000_000, WeeklyAllModels: 4_500_000, WeeklyOpusOnly: 0, DailyChatMessages: 200}
	default:
		c = Caps{SessionBlockTokens: 200_000, WeeklyAllModels: 1_000_000, WeeklyOpusOnly: 0, DailyChatMessages: 50}
	}
	if v := envInt("CYDMONITOR_SESSION_TOKENS"); v > 0 {
		c.SessionBlockTokens = v
	}
	if v := envInt("CYDMONITOR_WEEKLY_ALL"); v > 0 {
		c.WeeklyAllModels = v
	}
	if v := envInt("CYDMONITOR_WEEKLY_OPUS"); v > 0 {
		c.WeeklyOpusOnly = v
	}
	if v := envInt("CYDMONITOR_DAILY_CHAT_MESSAGES"); v > 0 {
		c.DailyChatMessages = v
	}
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
