package claudedata

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"time"
)

type Usage struct {
	Input         int
	Output        int
	CacheRead     int
	CacheCreation int
}

// Total returns the tokens that count against plan limits. Anthropic does
// not charge cache_read_input_tokens against your quota (they're heavily
// discounted), so they're excluded; only Input + Output + CacheCreation
// contribute to session blocks and weekly budgets.
func (u Usage) Total() int {
	return u.Input + u.Output + u.CacheCreation
}

type Record struct {
	SessionID string
	RequestID string
	MessageID string
	Timestamp time.Time
	Model     string
	Tokens    Usage
}

type rawRecord struct {
	SessionID string    `json:"sessionId"`
	RequestID string    `json:"requestId"`
	Timestamp time.Time `json:"timestamp"`
	Type      string    `json:"type"`
	Message   *struct {
		ID    string `json:"id"`
		Model string `json:"model"`
		Usage *struct {
			InputTokens              int `json:"input_tokens"`
			OutputTokens             int `json:"output_tokens"`
			CacheReadInputTokens     int `json:"cache_read_input_tokens"`
			CacheCreationInputTokens int `json:"cache_creation_input_tokens"`
		} `json:"usage"`
	} `json:"message"`
}

// ParseLine returns (record, kept, err). Lines that are valid JSON but represent
// non-assistant or usage-less messages return ok=false with nil error.
func ParseLine(line []byte) (Record, bool, error) {
	var raw rawRecord
	if err := json.Unmarshal(line, &raw); err != nil {
		return Record{}, false, fmt.Errorf("parse: %w", err)
	}
	if raw.Type != "assistant" || raw.Message == nil || raw.Message.Usage == nil {
		return Record{}, false, nil
	}
	return Record{
		SessionID: raw.SessionID,
		RequestID: raw.RequestID,
		MessageID: raw.Message.ID,
		Timestamp: raw.Timestamp,
		Model:     raw.Message.Model,
		Tokens: Usage{
			Input:         raw.Message.Usage.InputTokens,
			Output:        raw.Message.Usage.OutputTokens,
			CacheRead:     raw.Message.Usage.CacheReadInputTokens,
			CacheCreation: raw.Message.Usage.CacheCreationInputTokens,
		},
	}, true, nil
}

// Dedup removes duplicate assistant records that share the same
// (MessageID, RequestID) pair. Claude Code writes the same assistant turn to
// JSONL multiple times — once per streaming iteration, and again when the same
// session is resumed from a worktree — so summing raw records double-counts
// tokens. Records with an empty MessageID are kept as-is since they cannot be
// distinguished from each other.
func Dedup(records []Record) []Record {
	out := make([]Record, 0, len(records))
	seen := make(map[string]struct{}, len(records))
	for _, r := range records {
		if r.MessageID == "" {
			out = append(out, r)
			continue
		}
		key := r.MessageID + "\x00" + r.RequestID
		if _, ok := seen[key]; ok {
			continue
		}
		seen[key] = struct{}{}
		out = append(out, r)
	}
	return out
}

func ParseReader(r io.Reader) ([]Record, error) {
	scanner := bufio.NewScanner(r)
	scanner.Buffer(make([]byte, 64*1024), 1024*1024)
	var out []Record
	for scanner.Scan() {
		line := scanner.Bytes()
		if len(line) == 0 {
			continue
		}
		rec, ok, err := ParseLine(line)
		if err != nil || !ok {
			continue
		}
		out = append(out, rec)
	}
	return out, scanner.Err()
}

func ParseFile(path string) ([]Record, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	return ParseReader(f)
}
