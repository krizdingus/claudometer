package server

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"github.com/krizdingus/cydmonitor/daemon/internal/claudedata"
	"github.com/krizdingus/cydmonitor/daemon/internal/pairings"
	"github.com/krizdingus/cydmonitor/daemon/internal/routines"
	"github.com/krizdingus/cydmonitor/daemon/internal/stats"
)

type fakeRoutines struct{}

func (fakeRoutines) Get(ctx context.Context) ([]routines.Routine, error) { return nil, nil }

func newTestServer(t *testing.T) (*Server, string) {
	t.Helper()
	store, _ := pairings.NewStore(filepath.Join(t.TempDir(), "p.json"))
	tok, _ := store.Add("cyd-1", "test")
	codes := pairings.NewCodes(2 * time.Minute)
	agg := &stats.Aggregator{
		Records:  nil,
		PlanInfo: claudedata.PlanInfo{Plan: claudedata.PlanPro},
		Routines: fakeRoutines{},
		Now:      time.Now,
	}
	s := New(Config{
		Store:      store,
		Codes:      codes,
		Aggregator: agg,
		Version:    "test",
	})
	return s, tok
}

func TestStatsHandler_RequiresAuth(t *testing.T) {
	s, _ := newTestServer(t)
	rec := httptest.NewRecorder()
	req := httptest.NewRequest("GET", "/v1/stats", nil)
	s.Handler().ServeHTTP(rec, req)
	if rec.Code != http.StatusUnauthorized {
		t.Errorf("status = %d, want 401", rec.Code)
	}
}

func TestStatsHandler_ReturnsJSONWithValidToken(t *testing.T) {
	s, tok := newTestServer(t)
	rec := httptest.NewRecorder()
	req := httptest.NewRequest("GET", "/v1/stats", nil)
	req.Header.Set("Authorization", "Bearer "+tok)
	s.Handler().ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, body = %s", rec.Code, rec.Body.String())
	}
	if got := rec.Header().Get("Content-Type"); !strings.HasPrefix(got, "application/json") {
		t.Errorf("Content-Type = %q", got)
	}
	var out stats.Stats
	if err := json.Unmarshal(rec.Body.Bytes(), &out); err != nil {
		t.Fatalf("response not JSON: %v", err)
	}
	if out.Schema != stats.SchemaVersion {
		t.Errorf("Schema = %d, want %d", out.Schema, stats.SchemaVersion)
	}
}

func TestPairInitHandler_ReturnsCode(t *testing.T) {
	s, _ := newTestServer(t)
	rec := httptest.NewRecorder()
	body := strings.NewReader(`{"cyd_id":"cyd-NEW"}`)
	req := httptest.NewRequest("POST", "/v1/pair-init", body)
	s.Handler().ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, body = %s", rec.Code, rec.Body.String())
	}
	var resp struct {
		Code string `json:"code"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &resp); err != nil {
		t.Fatal(err)
	}
	if len(resp.Code) != 4 {
		t.Errorf("code length = %d, want 4", len(resp.Code))
	}
}

func TestPairVerifyHandler_IssuesToken(t *testing.T) {
	s, _ := newTestServer(t)

	// First: init pairing to get a code
	rec1 := httptest.NewRecorder()
	body1 := strings.NewReader(`{"cyd_id":"cyd-NEW"}`)
	req1 := httptest.NewRequest("POST", "/v1/pair-init", body1)
	s.Handler().ServeHTTP(rec1, req1)
	var initResp struct{ Code string `json:"code"` }
	_ = json.Unmarshal(rec1.Body.Bytes(), &initResp)

	// Then: verify
	rec2 := httptest.NewRecorder()
	body2 := strings.NewReader(`{"cyd_id":"cyd-NEW","code":"` + initResp.Code + `","name":"Office"}`)
	req2 := httptest.NewRequest("POST", "/v1/pair-verify", body2)
	s.Handler().ServeHTTP(rec2, req2)
	if rec2.Code != http.StatusOK {
		t.Fatalf("status = %d, body = %s", rec2.Code, rec2.Body.String())
	}
	var verifyResp struct{ Token string `json:"token"` }
	_ = json.Unmarshal(rec2.Body.Bytes(), &verifyResp)
	if len(verifyResp.Token) < 32 {
		t.Errorf("token too short: %d", len(verifyResp.Token))
	}
}

func TestPairVerifyHandler_RejectsBadCode(t *testing.T) {
	s, _ := newTestServer(t)
	rec := httptest.NewRecorder()
	body := strings.NewReader(`{"cyd_id":"cyd-X","code":"0000","name":""}`)
	req := httptest.NewRequest("POST", "/v1/pair-verify", body)
	s.Handler().ServeHTTP(rec, req)
	if rec.Code != http.StatusUnauthorized {
		t.Errorf("status = %d, want 401", rec.Code)
	}
}

func TestStatusHandler_ReturnsState(t *testing.T) {
	s, _ := newTestServer(t)
	rec := httptest.NewRecorder()
	req := httptest.NewRequest("GET", "/v1/status", nil)
	s.Handler().ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d", rec.Code)
	}
	var out struct {
		Version     string `json:"version"`
		PairedCount int    `json:"paired_count"`
		PendingCode string `json:"pending_code"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &out); err != nil {
		t.Fatal(err)
	}
	if out.Version != "test" {
		t.Errorf("Version = %q, want test", out.Version)
	}
	if out.PairedCount != 1 {
		t.Errorf("PairedCount = %d, want 1", out.PairedCount)
	}
}
