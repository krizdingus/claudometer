// Package config owns the daemon's persistent configuration at
// ~/.config/claudometer/config.json (both macOS and Linux). Caps() produces
// the claudedata.Caps the runner consumes: file override > matching
// CLAUDOMETER_* env var > plan-tier default from claudedata.PlanCaps.
package config

import (
	"encoding/json"
	"errors"
	"io/fs"
	"os"
	"path/filepath"
	"strconv"

	"github.com/krizdingus/claudometer/daemon/pkg/claudedata"
)

// Settings is the on-disk schema. A zero value for a cap-override field
// means "no override at this layer, fall through to env/plan default."
type Settings struct {
	PlanTier                  string `json:"plan_tier"`
	SessionTokensOverride     int    `json:"session_tokens_override"`
	WeeklyAllOverride         int    `json:"weekly_all_override"`
	WeeklyOpusOverride        int    `json:"weekly_opus_override"`
	DailyChatMessagesOverride int    `json:"daily_chat_messages_override"`
	ListenAddr                string `json:"listen_addr"`
}

func defaults() Settings {
	return Settings{
		PlanTier:   "free",
		ListenAddr: "0.0.0.0:7842",
	}
}

// Load reads config.json. Missing file returns defaults (no error).
// Malformed JSON returns an error.
func Load(path string) (Settings, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		if errors.Is(err, fs.ErrNotExist) {
			return defaults(), nil
		}
		return Settings{}, err
	}
	out := defaults()
	if err := json.Unmarshal(data, &out); err != nil {
		return Settings{}, err
	}
	if out.PlanTier == "" {
		out.PlanTier = "free"
	}
	if out.ListenAddr == "" {
		out.ListenAddr = "0.0.0.0:7842"
	}
	return out, nil
}

// Save atomically writes config.json, creating parent directories as needed.
func Save(path string, s Settings) error {
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return err
	}
	data, err := json.MarshalIndent(s, "", "  ")
	if err != nil {
		return err
	}
	tmp := path + ".tmp"
	if err := os.WriteFile(tmp, data, 0o600); err != nil {
		return err
	}
	return os.Rename(tmp, path)
}

// EnsureExists creates a default config file at path if it does not exist.
// Returns the resolved Settings (whether freshly written or already present).
func EnsureExists(path string) (Settings, error) {
	s, err := Load(path)
	if err != nil {
		return Settings{}, err
	}
	if _, statErr := os.Stat(path); errors.Is(statErr, fs.ErrNotExist) {
		if err := Save(path, s); err != nil {
			return Settings{}, err
		}
	}
	return s, nil
}

// Caps resolves the final cap values for the configured plan, applying file
// overrides first, then matching CLAUDOMETER_* env vars, then plan defaults.
func (s Settings) Caps() claudedata.Caps {
	c := claudedata.PlanCaps(claudedata.Plan(s.PlanTier))
	apply := func(target *int, fileVal int, envKey string) {
		if fileVal > 0 {
			*target = fileVal
			return
		}
		if v := envInt(envKey); v > 0 {
			*target = v
		}
	}
	apply(&c.SessionBlockTokens, s.SessionTokensOverride, "CLAUDOMETER_SESSION_TOKENS")
	apply(&c.WeeklyAllModels, s.WeeklyAllOverride, "CLAUDOMETER_WEEKLY_ALL")
	apply(&c.WeeklyOpusOnly, s.WeeklyOpusOverride, "CLAUDOMETER_WEEKLY_OPUS")
	apply(&c.DailyChatMessages, s.DailyChatMessagesOverride, "CLAUDOMETER_DAILY_CHAT_MESSAGES")
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

// DefaultPath returns ~/.config/claudometer/config.json on both macOS and Linux.
func DefaultPath() (string, error) {
	home, err := os.UserHomeDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(home, ".config", "claudometer", "config.json"), nil
}
