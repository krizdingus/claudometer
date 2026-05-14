package pairings

import (
	"testing"
	"time"
)

func TestCodes_RequestReturnsFourDigitCode(t *testing.T) {
	c := NewCodes(2 * time.Minute)
	req, err := c.Request("cyd-X")
	if err != nil {
		t.Fatal(err)
	}
	if len(req.Code) != 4 {
		t.Errorf("code length = %d, want 4", len(req.Code))
	}
	for _, ch := range req.Code {
		if ch < '0' || ch > '9' {
			t.Errorf("code contains non-digit: %q", req.Code)
		}
	}
}

func TestCodes_VerifyAcceptsCorrectCode(t *testing.T) {
	c := NewCodes(2 * time.Minute)
	req, _ := c.Request("cyd-X")
	cydID, ok := c.Verify(req.Code)
	if !ok {
		t.Fatal("Verify should accept correct code")
	}
	if cydID != "cyd-X" {
		t.Errorf("cydID = %q, want cyd-X", cydID)
	}
}

func TestCodes_VerifyRejectsWrongCode(t *testing.T) {
	c := NewCodes(2 * time.Minute)
	_, _ = c.Request("cyd-X")
	if _, ok := c.Verify("0000"); ok {
		t.Errorf("Verify should reject wrong code")
	}
}

func TestCodes_CodeExpires(t *testing.T) {
	c := NewCodes(1 * time.Second)
	now := time.Unix(1000, 0)
	c.Now = func() time.Time { return now }
	req, _ := c.Request("cyd-X")
	now = now.Add(2 * time.Second)
	if _, ok := c.Verify(req.Code); ok {
		t.Errorf("Verify should reject expired code")
	}
}

func TestCodes_VerifyConsumesCode(t *testing.T) {
	c := NewCodes(2 * time.Minute)
	req, _ := c.Request("cyd-X")
	if _, ok := c.Verify(req.Code); !ok {
		t.Fatal("first verify should succeed")
	}
	if _, ok := c.Verify(req.Code); ok {
		t.Errorf("second verify of same code should fail")
	}
}

func TestCodes_Pending(t *testing.T) {
	c := NewCodes(2 * time.Minute)
	if c.Pending() != "" {
		t.Errorf("Pending should be empty initially")
	}
	req, _ := c.Request("cyd-X")
	if c.Pending() != req.Code {
		t.Errorf("Pending = %q, want %q", c.Pending(), req.Code)
	}
}
