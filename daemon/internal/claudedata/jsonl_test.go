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
