package routines

import (
	"context"
	"encoding/json"
	"os/exec"
	"sync"
	"time"
)

type Routine struct {
	Name       string     `json:"name"`
	Schedule   string     `json:"schedule"`
	LastRun    *time.Time `json:"last_run"`
	LastStatus string     `json:"last_status"`
	NextRun    *time.Time `json:"next_run"`
}

func Parse(data []byte) ([]Routine, error) {
	var out []Routine
	if err := json.Unmarshal(data, &out); err != nil {
		return nil, err
	}
	return out, nil
}

// execFunc is the shape we mock in tests.
type execFunc func(ctx context.Context) ([]byte, error)

// Fetcher caches the result of `claude routines list --json` for TTL.
type Fetcher struct {
	TTL time.Duration
	Now func() time.Time
	exec execFunc

	mu        sync.Mutex
	cached    []Routine
	cachedAt  time.Time
	cachedErr error
}

// NewFetcher returns a Fetcher that shells out to the `claude` CLI.
func NewFetcher(ttl time.Duration) *Fetcher {
	return &Fetcher{
		TTL: ttl,
		Now: time.Now,
		exec: func(ctx context.Context) ([]byte, error) {
			cmd := exec.CommandContext(ctx, "claude", "routines", "list", "--json")
			return cmd.Output()
		},
	}
}

func (f *Fetcher) Get(ctx context.Context) ([]Routine, error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	if !f.cachedAt.IsZero() && f.Now().Sub(f.cachedAt) < f.TTL {
		return f.cached, f.cachedErr
	}
	data, err := f.exec(ctx)
	if err != nil {
		f.cached, f.cachedErr, f.cachedAt = nil, err, f.Now()
		return nil, err
	}
	parsed, perr := Parse(data)
	f.cached, f.cachedErr, f.cachedAt = parsed, perr, f.Now()
	return parsed, perr
}
