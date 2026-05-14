package main

import (
	"context"
	"encoding/json"
	"io"
	"net/http"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

// TestE2E_PairAndFetchStats runs the full daemon binary, performs the pairing
// handshake, and fetches /v1/stats. Skipped under `go test -short`.
func TestE2E_PairAndFetchStats(t *testing.T) {
	if testing.Short() {
		t.Skip("skipping e2e in -short mode")
	}
	// Build the binary fresh
	bin := filepath.Join(t.TempDir(), "claudometer")
	if out, err := exec.Command("go", "build", "-o", bin, ".").CombinedOutput(); err != nil {
		t.Fatalf("build: %v\n%s", err, out)
	}

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	cmd := exec.CommandContext(ctx, bin)
	cmd.Env = append(cmd.Env, "HOME="+t.TempDir())
	if err := cmd.Start(); err != nil {
		t.Fatalf("start: %v", err)
	}
	defer func() { _ = cmd.Process.Kill() }()

	// Wait for server
	waitForReady(t, "http://127.0.0.1:7842/v1/status", 3*time.Second)

	// Init pairing
	initBody := strings.NewReader(`{"cyd_id":"cyd-e2e"}`)
	initResp, err := http.Post("http://127.0.0.1:7842/v1/pair-init", "application/json", initBody)
	if err != nil {
		t.Fatal(err)
	}
	var initOut struct{ Code string `json:"code"` }
	_ = json.NewDecoder(initResp.Body).Decode(&initOut)
	initResp.Body.Close()
	if len(initOut.Code) != 4 {
		t.Fatalf("got code %q", initOut.Code)
	}

	// Verify pairing
	verifyBody := strings.NewReader(`{"cyd_id":"cyd-e2e","code":"` + initOut.Code + `","name":"E2E"}`)
	vResp, err := http.Post("http://127.0.0.1:7842/v1/pair-verify", "application/json", verifyBody)
	if err != nil {
		t.Fatal(err)
	}
	var vOut struct{ Token string `json:"token"` }
	_ = json.NewDecoder(vResp.Body).Decode(&vOut)
	vResp.Body.Close()
	if len(vOut.Token) < 32 {
		t.Fatalf("got token %q", vOut.Token)
	}

	// Fetch stats
	req, _ := http.NewRequest("GET", "http://127.0.0.1:7842/v1/stats", nil)
	req.Header.Set("Authorization", "Bearer "+vOut.Token)
	sResp, err := http.DefaultClient.Do(req)
	if err != nil {
		t.Fatal(err)
	}
	defer sResp.Body.Close()
	if sResp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(sResp.Body)
		t.Fatalf("stats status = %d, body = %s", sResp.StatusCode, body)
	}
	var stats map[string]interface{}
	_ = json.NewDecoder(sResp.Body).Decode(&stats)
	if stats["schema"] == nil {
		t.Errorf("response missing schema field")
	}
}

func waitForReady(t *testing.T, url string, timeout time.Duration) {
	t.Helper()
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		resp, err := http.Get(url)
		if err == nil {
			resp.Body.Close()
			if resp.StatusCode == 200 {
				return
			}
		}
		time.Sleep(50 * time.Millisecond)
	}
	t.Fatalf("server never reached ready at %s", url)
}
