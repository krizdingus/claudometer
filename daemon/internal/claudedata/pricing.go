package claudedata

// ModelPrice is dollars per 1M tokens.
type ModelPrice struct {
	InputPer1M         float64
	OutputPer1M        float64
	CacheReadPer1M     float64
	CacheCreationPer1M float64
}

// Pricing is a snapshot of Anthropic's published prices.
// Update when prices change; the version field in /v1/stats can signal staleness.
var Pricing = map[string]ModelPrice{
	"claude-opus-4-7":   {InputPer1M: 15.00, OutputPer1M: 75.00, CacheReadPer1M: 1.50, CacheCreationPer1M: 18.75},
	"claude-sonnet-4-6": {InputPer1M: 3.00, OutputPer1M: 15.00, CacheReadPer1M: 0.30, CacheCreationPer1M: 3.75},
	"claude-haiku-4-5":  {InputPer1M: 1.00, OutputPer1M: 5.00, CacheReadPer1M: 0.10, CacheCreationPer1M: 1.25},
}

func Estimate(model string, u Usage) float64 {
	p, ok := Pricing[model]
	if !ok {
		return 0
	}
	per := func(toks int, rate float64) float64 { return float64(toks) * rate / 1_000_000 }
	return per(u.Input, p.InputPer1M) +
		per(u.Output, p.OutputPer1M) +
		per(u.CacheRead, p.CacheReadPer1M) +
		per(u.CacheCreation, p.CacheCreationPer1M)
}
