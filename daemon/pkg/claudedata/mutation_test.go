package claudedata_test

import (
	"testing"
	"time"

	cd "github.com/krizdingus/cydmonitor/daemon/pkg/claudedata"
)

func TestMutationSafety(t *testing.T) {
	// Verify that ComputeBlocks does not mutate the input slice
	original := []cd.Record{
		{Timestamp: time.Unix(100, 0), Model: "sonnet", Tokens: cd.Usage{Input: 10}},
		{Timestamp: time.Unix(50, 0), Model: "opus", Tokens: cd.Usage{Input: 20}},
		{Timestamp: time.Unix(150, 0), Model: "sonnet", Tokens: cd.Usage{Input: 5}},
	}

	originalCopy := make([]cd.Record, len(original))
	copy(originalCopy, original)

	_ = cd.ComputeBlocks(original, 1*time.Hour)

	// Verify input wasn't mutated
	for i := range original {
		if original[i].Timestamp != originalCopy[i].Timestamp {
			t.Errorf("Timestamp at index %d was mutated", i)
		}
		if original[i].Model != originalCopy[i].Model {
			t.Errorf("Model at index %d was mutated", i)
		}
	}
}

func TestPointerStability(t *testing.T) {
	// Verify that ActiveBlock returns stable pointers
	recs := []cd.Record{
		{Timestamp: time.Unix(100, 0), Model: "sonnet", Tokens: cd.Usage{Input: 10}},
	}
	blocks := cd.ComputeBlocks(recs, 1*time.Hour)

	p1 := cd.ActiveBlock(blocks, time.Unix(150, 0))
	p2 := cd.ActiveBlock(blocks, time.Unix(150, 0))

	if p1 != p2 {
		t.Errorf("ActiveBlock returned different pointers for same query")
	}
}
