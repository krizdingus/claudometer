package server

import (
	"context"
	"encoding/json"
	"net"
	"net/http"

	"github.com/krizdingus/claudometer/daemon/pkg/pairings"
	"github.com/krizdingus/claudometer/daemon/pkg/stats"
)

type aggregator interface {
	Build(ctx context.Context) (stats.Stats, error)
}

type Config struct {
	Store      *pairings.Store
	Codes      *pairings.Codes
	Aggregator aggregator
	Version    string
}

type Server struct {
	cfg Config
}

func New(cfg Config) *Server { return &Server{cfg: cfg} }

func (s *Server) Handler() http.Handler {
	mux := http.NewServeMux()

	// Authenticated:
	mux.Handle("GET /v1/stats", RequireToken(s.cfg.Store, http.HandlerFunc(s.stats)))

	// Unauthenticated:
	mux.HandleFunc("POST /v1/pair-init", s.pairInit)
	mux.HandleFunc("POST /v1/pair-verify", s.pairVerify)
	mux.HandleFunc("GET /v1/status", s.status)

	// Loopback-only:
	mux.HandleFunc("POST /v1/admin/pair", s.adminPair)

	return mux
}

func (s *Server) stats(w http.ResponseWriter, r *http.Request) {
	out, err := s.cfg.Aggregator.Build(r.Context())
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	writeJSON(w, http.StatusOK, out)
}

type pairInitReq struct {
	CydID string `json:"cyd_id"`
}
type pairInitResp struct {
	Code string `json:"code"`
}

func (s *Server) pairInit(w http.ResponseWriter, r *http.Request) {
	var req pairInitReq
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil || req.CydID == "" {
		http.Error(w, "bad request", http.StatusBadRequest)
		return
	}
	pending, err := s.cfg.Codes.Request(req.CydID)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	writeJSON(w, http.StatusOK, pairInitResp{Code: pending.Code})
}

type pairVerifyReq struct {
	CydID string `json:"cyd_id"`
	Code  string `json:"code"`
	Name  string `json:"name"`
}
type pairVerifyResp struct {
	Token string `json:"token"`
}

func (s *Server) pairVerify(w http.ResponseWriter, r *http.Request) {
	var req pairVerifyReq
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, "bad request", http.StatusBadRequest)
		return
	}
	cydID, ok := s.cfg.Codes.Verify(req.Code)
	if !ok || cydID != req.CydID {
		http.Error(w, "invalid code", http.StatusUnauthorized)
		return
	}
	name := req.Name
	if name == "" {
		name = cydID
	}
	tok, err := s.cfg.Store.Add(cydID, name)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	writeJSON(w, http.StatusOK, pairVerifyResp{Token: tok})
}

type statusResp struct {
	Version     string `json:"version"`
	PairedCount int    `json:"paired_count"`
	PendingCode string `json:"pending_code"`
}

func (s *Server) status(w http.ResponseWriter, _ *http.Request) {
	writeJSON(w, http.StatusOK, statusResp{
		Version:     s.cfg.Version,
		PairedCount: len(s.cfg.Store.List()),
		PendingCode: s.cfg.Codes.Pending(),
	})
}

func writeJSON(w http.ResponseWriter, code int, v interface{}) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(code)
	_ = json.NewEncoder(w).Encode(v)
}

type adminPairReq struct {
	CydID string `json:"cyd_id"`
	Name  string `json:"name"`
}

type adminPairResp struct {
	Token string `json:"token"`
}

func (s *Server) adminPair(w http.ResponseWriter, r *http.Request) {
	if !isLoopback(r.RemoteAddr) {
		http.Error(w, "forbidden", http.StatusForbidden)
		return
	}
	var req adminPairReq
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil || req.CydID == "" {
		http.Error(w, "bad request", http.StatusBadRequest)
		return
	}
	name := req.Name
	if name == "" {
		name = req.CydID
	}
	tok, err := s.cfg.Store.Add(req.CydID, name)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	writeJSON(w, http.StatusOK, adminPairResp{Token: tok})
}

func isLoopback(remoteAddr string) bool {
	host, _, err := net.SplitHostPort(remoteAddr)
	if err != nil {
		host = remoteAddr
	}
	ip := net.ParseIP(host)
	if ip == nil {
		return false
	}
	return ip.IsLoopback()
}
