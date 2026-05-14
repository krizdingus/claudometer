package cli_test

import (
	"bytes"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/krizdingus/claudometer/daemon/pkg/cli"
)

func TestSetPlan_WritesAndRestarts(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "config.json")
	os.WriteFile(path, []byte(`{"plan_tier":"free"}`), 0o600)

	var restartCalls int
	out := &bytes.Buffer{}
	code := cli.SetPlanWith(out, path, "max-5x", func() error {
		restartCalls++
		return nil
	})
	if code != 0 {
		t.Fatalf("exit code = %d, want 0; out=%s", code, out)
	}
	if restartCalls != 1 {
		t.Errorf("restartCalls = %d, want 1", restartCalls)
	}
	data, _ := os.ReadFile(path)
	if !strings.Contains(string(data), `"plan_tier": "max-5x"`) {
		t.Errorf("plan not written; got: %s", data)
	}
}

func TestSetPlan_RejectsInvalidTier(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "config.json")
	os.WriteFile(path, []byte(`{"plan_tier":"free"}`), 0o600)

	out := &bytes.Buffer{}
	code := cli.SetPlanWith(out, path, "ultra-plus", func() error { return nil })
	if code == 0 {
		t.Errorf("exit code = 0, want non-zero for invalid plan")
	}
	if !strings.Contains(out.String(), "invalid plan") {
		t.Errorf("expected error about invalid plan; got: %s", out.String())
	}
}

func TestSetPlan_CreatesConfigIfMissing(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "config.json")

	out := &bytes.Buffer{}
	code := cli.SetPlanWith(out, path, "max-5x", func() error { return nil })
	if code != 0 {
		t.Fatalf("exit code = %d, want 0; out=%s", code, out)
	}
	if _, err := os.Stat(path); err != nil {
		t.Errorf("config not created: %v", err)
	}
}
