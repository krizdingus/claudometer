package routines

import (
	"context"
	"os"
	"testing"
	"time"
)

func TestParse_ReadsFixture(t *testing.T) {
	data, err := os.ReadFile("../../testdata/routines/list-output.json")
	if err != nil {
		t.Fatal(err)
	}
	routines, err := Parse(data)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	if len(routines) != 5 {
		t.Fatalf("got %d routines, want 5", len(routines))
	}
	if routines[0].Name != "babysit-prs" {
		t.Errorf("routines[0].Name = %q, want babysit-prs", routines[0].Name)
	}
	if routines[0].LastStatus != "ok" {
		t.Errorf("routines[0].LastStatus = %q, want ok", routines[0].LastStatus)
	}
	if routines[4].LastRun != nil {
		t.Errorf("routines[4].LastRun should be nil (never ran), got %v", routines[4].LastRun)
	}
}

func TestFetcher_CachesWithinTTL(t *testing.T) {
	calls := 0
	f := &Fetcher{
		TTL: 1 * time.Hour,
		Now: func() time.Time { return time.Unix(1000, 0) },
		exec: func(ctx context.Context) ([]byte, error) {
			calls++
			return []byte("[]"), nil
		},
	}
	_, _ = f.Get(context.Background())
	_, _ = f.Get(context.Background())
	if calls != 1 {
		t.Errorf("exec called %d times, want 1 (second call should be cached)", calls)
	}
}

func TestFetcher_RefetchesAfterTTL(t *testing.T) {
	calls := 0
	now := time.Unix(1000, 0)
	f := &Fetcher{
		TTL: 60 * time.Second,
		Now: func() time.Time { return now },
		exec: func(ctx context.Context) ([]byte, error) {
			calls++
			return []byte("[]"), nil
		},
	}
	_, _ = f.Get(context.Background())
	now = now.Add(61 * time.Second)
	_, _ = f.Get(context.Background())
	if calls != 2 {
		t.Errorf("exec called %d times, want 2 (cache should have expired)", calls)
	}
}
