package claudedata

import (
	"strings"
	"testing"
	"time"
)

func TestParseLine_AssistantWithUsage(t *testing.T) {
	line := `{"sessionId":"alpha-1","timestamp":"2026-05-13T09:00:02Z","type":"assistant","message":{"id":"msg_1","model":"claude-sonnet-4-6","usage":{"input_tokens":120,"output_tokens":340,"cache_read_input_tokens":5000,"cache_creation_input_tokens":1000}}}`
	r, ok, err := ParseLine([]byte(line))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !ok {
		t.Fatalf("expected record to be kept, got skipped")
	}
	if r.SessionID != "alpha-1" {
		t.Errorf("SessionID = %q, want alpha-1", r.SessionID)
	}
	want, _ := time.Parse(time.RFC3339, "2026-05-13T09:00:02Z")
	if !r.Timestamp.Equal(want) {
		t.Errorf("Timestamp = %v, want %v", r.Timestamp, want)
	}
	if r.Model != "claude-sonnet-4-6" {
		t.Errorf("Model = %q", r.Model)
	}
	if r.Tokens.Input != 120 || r.Tokens.Output != 340 ||
		r.Tokens.CacheRead != 5000 || r.Tokens.CacheCreation != 1000 {
		t.Errorf("Tokens = %+v", r.Tokens)
	}
	if r.MessageID != "msg_1" {
		t.Errorf("MessageID = %q, want msg_1", r.MessageID)
	}
}

func TestParseLine_CapturesRequestID(t *testing.T) {
	line := `{"sessionId":"s","requestId":"req_abc","timestamp":"2026-05-13T09:00:02Z","type":"assistant","message":{"id":"msg_x","model":"claude-opus-4-7","usage":{"input_tokens":1,"output_tokens":1}}}`
	r, ok, err := ParseLine([]byte(line))
	if err != nil || !ok {
		t.Fatalf("ParseLine err=%v ok=%v", err, ok)
	}
	if r.RequestID != "req_abc" || r.MessageID != "msg_x" {
		t.Errorf("RequestID=%q MessageID=%q", r.RequestID, r.MessageID)
	}
}

func TestDedup_RemovesDuplicateMessageIDs(t *testing.T) {
	// Claude Code logs the same assistant turn multiple times within a single
	// JSONL file (once per streaming iteration). Dedup must collapse those to
	// one record per (MessageID, RequestID) pair.
	recs := []Record{
		{MessageID: "msg_1", RequestID: "req_a", Tokens: Usage{Input: 100}},
		{MessageID: "msg_1", RequestID: "req_a", Tokens: Usage{Input: 100}},
		{MessageID: "msg_1", RequestID: "req_a", Tokens: Usage{Input: 100}},
		{MessageID: "msg_2", RequestID: "req_b", Tokens: Usage{Input: 50}},
	}
	out := Dedup(recs)
	if len(out) != 2 {
		t.Fatalf("Dedup len = %d, want 2", len(out))
	}
	total := 0
	for _, r := range out {
		total += r.Tokens.Total()
	}
	if total != 150 {
		t.Errorf("total tokens = %d, want 150", total)
	}
}

func TestDedup_KeepsRecordsWithoutMessageID(t *testing.T) {
	// Older Claude Code data or non-standard records may lack a message id;
	// we can't tell them apart so we keep every one rather than collapse.
	recs := []Record{
		{MessageID: "", Tokens: Usage{Input: 10}},
		{MessageID: "", Tokens: Usage{Input: 20}},
		{MessageID: "msg_1", Tokens: Usage{Input: 5}},
		{MessageID: "msg_1", Tokens: Usage{Input: 5}},
	}
	out := Dedup(recs)
	if len(out) != 3 {
		t.Fatalf("Dedup len = %d, want 3", len(out))
	}
}

func TestDedup_DistinguishesSameMessageIDDifferentRequestID(t *testing.T) {
	recs := []Record{
		{MessageID: "msg_1", RequestID: "req_a", Tokens: Usage{Input: 10}},
		{MessageID: "msg_1", RequestID: "req_b", Tokens: Usage{Input: 10}},
	}
	out := Dedup(recs)
	if len(out) != 2 {
		t.Errorf("Dedup len = %d, want 2 (different requestIds)", len(out))
	}
}

func TestParseLine_UserMessageSkipped(t *testing.T) {
	line := `{"sessionId":"alpha-1","timestamp":"2026-05-13T09:00:00Z","type":"user","message":{"content":"hi"}}`
	_, ok, err := ParseLine([]byte(line))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if ok {
		t.Errorf("expected user record to be skipped")
	}
}

func TestParseLine_MalformedReturnsError(t *testing.T) {
	_, _, err := ParseLine([]byte("not json"))
	if err == nil {
		t.Errorf("expected error for malformed line")
	}
}

func TestParseFile_ReadsFixture(t *testing.T) {
	records, err := ParseFile("../../testdata/claude/projects/proj-alpha.jsonl")
	if err != nil {
		t.Fatalf("ParseFile: %v", err)
	}
	if len(records) != 3 {
		t.Errorf("got %d records, want 3 (one user line should be skipped)", len(records))
	}
}

func TestParseFile_SkipsCorruptLines(t *testing.T) {
	corrupt := "not json\n" +
		`{"sessionId":"x","timestamp":"2026-05-13T09:00:00Z","type":"assistant","message":{"id":"m","model":"claude-haiku-4-5","usage":{"input_tokens":1,"output_tokens":1}}}` + "\n"
	records, err := ParseReader(strings.NewReader(corrupt))
	if err != nil {
		t.Fatalf("ParseReader: %v", err)
	}
	if len(records) != 1 {
		t.Errorf("want 1 record (malformed line skipped), got %d", len(records))
	}
}
