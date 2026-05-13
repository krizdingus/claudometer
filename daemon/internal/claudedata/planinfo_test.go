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
