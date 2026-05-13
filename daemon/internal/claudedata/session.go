package claudedata

import (
	"sort"
	"time"
)

type Block struct {
	Start      time.Time
	End        time.Time
	Records    []Record
	TokensUsed int
	PerModel   map[string]int
}

// ComputeBlocks groups records into time-bounded blocks. Each block starts at
// the timestamp of its first record and ends at start+duration. Records whose
// timestamps fall outside the current block's window open a new block.
func ComputeBlocks(records []Record, duration time.Duration) []Block {
	if len(records) == 0 {
		return nil
	}
	sorted := make([]Record, len(records))
	copy(sorted, records)
	sort.SliceStable(sorted, func(i, j int) bool {
		return sorted[i].Timestamp.Before(sorted[j].Timestamp)
	})

	var blocks []Block
	for _, r := range sorted {
		if len(blocks) == 0 || !r.Timestamp.Before(blocks[len(blocks)-1].End) {
			blocks = append(blocks, Block{
				Start:    r.Timestamp,
				End:      r.Timestamp.Add(duration),
				PerModel: map[string]int{},
			})
		}
		cur := &blocks[len(blocks)-1]
		cur.Records = append(cur.Records, r)
		cur.TokensUsed += r.Tokens.Total()
		cur.PerModel[r.Model] += r.Tokens.Total()
	}
	return blocks
}

// ActiveBlock returns the block whose [Start, End) window contains now,
// or nil if no such block exists.
func ActiveBlock(blocks []Block, now time.Time) *Block {
	for i := range blocks {
		if !now.Before(blocks[i].Start) && now.Before(blocks[i].End) {
			return &blocks[i]
		}
	}
	return nil
}
