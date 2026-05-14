package claudedata

import (
	"math"
	"testing"
)

func near(a, b float64) bool { return math.Abs(a-b) < 0.0001 }

func TestEstimate_SonnetInputOutput(t *testing.T) {
	// Sonnet 4.6: $3/M input, $15/M output
	// 1M input + 100k output = 3 + 1.5 = $4.50
	cost := Estimate("claude-sonnet-4-6", Usage{Input: 1_000_000, Output: 100_000})
	if !near(cost, 4.50) {
		t.Errorf("Sonnet cost = %f, want ~4.50", cost)
	}
}

func TestEstimate_OpusWithCache(t *testing.T) {
	// Opus 4.7: $15/M input, $75/M output, $1.50/M cache read, $18.75/M cache creation
	// 100k input + 50k output + 1M cache read + 100k cache creation
	// = 1.5 + 3.75 + 1.5 + 1.875 = 8.625
	cost := Estimate("claude-opus-4-7", Usage{
		Input: 100_000, Output: 50_000,
		CacheRead: 1_000_000, CacheCreation: 100_000,
	})
	if !near(cost, 8.625) {
		t.Errorf("Opus cost = %f, want ~8.625", cost)
	}
}

func TestEstimate_UnknownModelReturnsZero(t *testing.T) {
	cost := Estimate("claude-unknown-99", Usage{Input: 1000, Output: 1000})
	if cost != 0 {
		t.Errorf("unknown model cost = %f, want 0", cost)
	}
}
