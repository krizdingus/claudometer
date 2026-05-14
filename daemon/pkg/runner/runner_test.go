package runner_test

import (
	"context"
	"net/http"
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/krizdingus/claudometer/daemon/pkg/claudedata"
	"github.com/krizdingus/claudometer/daemon/pkg/runner"
)

func TestService_StartAndStop(t *testing.T) {
	dir := t.TempDir()
	projects := filepath.Join(dir, "projects")
	if err := os.MkdirAll(projects, 0o755); err != nil {
		t.Fatal(err)
	}
	pairingsPath := filepath.Join(dir, "pairings.json")

	svc, err := runner.New(runner.Options{
		ListenAddr:     "127.0.0.1:0",
		ProjectsDir:    projects,
		PairingsPath:   pairingsPath,
		PlanInfo:       claudedata.PlanInfo{Plan: claudedata.PlanFree},
		Caps:           claudedata.PlanCaps(claudedata.PlanFree),
		ReloadInterval: 10 * time.Millisecond,
		Version:        "test",
	})
	if err != nil {
		t.Fatalf("New: %v", err)
	}

	ctx, cancel := context.WithCancel(context.Background())
	t.Cleanup(cancel)

	errCh := make(chan error, 1)
	go func() { errCh <- svc.Start(ctx) }()

	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) {
		if svc.Addr() != "" {
			break
		}
		time.Sleep(5 * time.Millisecond)
	}
	if svc.Addr() == "" {
		t.Fatal("server didn't bind a port within 2s")
	}

	resp, err := http.Get("http://" + svc.Addr() + "/v1/status")
	if err != nil {
		t.Fatalf("GET /v1/status: %v", err)
	}
	resp.Body.Close()
	if resp.StatusCode != 200 {
		t.Fatalf("status %d, want 200", resp.StatusCode)
	}

	cancel()
	select {
	case err := <-errCh:
		if err != nil && err != context.Canceled {
			t.Fatalf("Start returned %v, want nil or context.Canceled", err)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("Service.Start didn't return within 2s of cancel")
	}
}
