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
