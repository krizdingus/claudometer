package config_test

import (
	"path/filepath"
	"testing"

	"github.com/krizdingus/claudometer/daemon/pkg/claudedata"
	"github.com/krizdingus/claudometer/daemon/pkg/config"
)

func TestLoad_MissingFileReturnsDefaults(t *testing.T) {
	path := filepath.Join(t.TempDir(), "config.json")
	s, err := config.Load(path)
	if err != nil {
		t.Fatalf("Load: %v", err)
	}
	if s.PlanTier != "free" {
		t.Errorf("PlanTier = %q, want free", s.PlanTier)
	}
	if s.ListenAddr != "0.0.0.0:7842" {
		t.Errorf("ListenAddr = %q, want 0.0.0.0:7842", s.ListenAddr)
	}
}

func TestSaveLoad_RoundTrip(t *testing.T) {
	path := filepath.Join(t.TempDir(), "config.json")
	want := config.Settings{
		PlanTier:           "max-5x",
		WeeklyAllOverride:  45_000_000,
		WeeklyOpusOverride: 9_000_000,
		ListenAddr:         "127.0.0.1:7842",
	}
	if err := config.Save(path, want); err != nil {
		t.Fatalf("Save: %v", err)
	}
	got, err := config.Load(path)
	if err != nil {
		t.Fatalf("Load: %v", err)
	}
	if got.PlanTier != want.PlanTier ||
		got.WeeklyAllOverride != want.WeeklyAllOverride ||
		got.WeeklyOpusOverride != want.WeeklyOpusOverride ||
		got.ListenAddr != want.ListenAddr {
		t.Errorf("got %+v, want %+v", got, want)
	}
}

func TestCaps_FileOverridesEnv(t *testing.T) {
	t.Setenv("CLAUDOMETER_WEEKLY_ALL", "12345")
	s := config.Settings{PlanTier: "max-5x", WeeklyAllOverride: 99_999_999}
	c := s.Caps()
	if c.WeeklyAllModels != 99_999_999 {
		t.Errorf("WeeklyAllModels = %d, want file override 99999999", c.WeeklyAllModels)
	}
}

func TestCaps_EnvOverridesPlanDefault(t *testing.T) {
	t.Setenv("CLAUDOMETER_WEEKLY_ALL", "777")
	s := config.Settings{PlanTier: "max-5x"}
	c := s.Caps()
	if c.WeeklyAllModels != 777 {
		t.Errorf("WeeklyAllModels = %d, want env override 777", c.WeeklyAllModels)
	}
}

func TestCaps_FallsBackToPlanDefault(t *testing.T) {
	t.Setenv("CLAUDOMETER_WEEKLY_ALL", "")
	t.Setenv("CLAUDOMETER_SESSION_TOKENS", "")
	t.Setenv("CLAUDOMETER_WEEKLY_OPUS", "")
	t.Setenv("CLAUDOMETER_DAILY_CHAT_MESSAGES", "")
	s := config.Settings{PlanTier: "max-5x"}
	c := s.Caps()
	want := claudedata.PlanCaps(claudedata.PlanMax5x)
	if c != want {
		t.Errorf("Caps = %+v, want plan default %+v", c, want)
	}
}

func TestEnsureExists_CreatesDefaultFile(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "claudometer", "config.json")
	s, err := config.EnsureExists(path)
	if err != nil {
		t.Fatalf("EnsureExists: %v", err)
	}
	if s.PlanTier != "free" {
		t.Errorf("PlanTier = %q, want free", s.PlanTier)
	}
	if _, err := config.Load(path); err != nil {
		t.Errorf("Load after EnsureExists: %v", err)
	}
}
