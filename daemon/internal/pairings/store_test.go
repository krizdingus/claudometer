package pairings

import (
	"path/filepath"
	"testing"
)

func TestStore_AddAndLookup(t *testing.T) {
	dir := t.TempDir()
	s, err := NewStore(filepath.Join(dir, "pairings.json"))
	if err != nil {
		t.Fatal(err)
	}
	tok, err := s.Add("cyd-A1B2", "Living Room CYD")
	if err != nil {
		t.Fatal(err)
	}
	if len(tok) < 32 {
		t.Errorf("token too short: %d chars", len(tok))
	}
	pair, ok := s.Lookup(tok)
	if !ok {
		t.Fatal("Lookup failed for issued token")
	}
	if pair.CydID != "cyd-A1B2" {
		t.Errorf("CydID = %q, want cyd-A1B2", pair.CydID)
	}
}

func TestStore_PersistsAcrossInstances(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "pairings.json")

	s1, _ := NewStore(path)
	tok, _ := s1.Add("cyd-1", "Test")

	s2, err := NewStore(path)
	if err != nil {
		t.Fatal(err)
	}
	if _, ok := s2.Lookup(tok); !ok {
		t.Errorf("token not persisted across NewStore calls")
	}
}

func TestStore_Reset(t *testing.T) {
	dir := t.TempDir()
	s, _ := NewStore(filepath.Join(dir, "pairings.json"))
	tok, _ := s.Add("cyd-1", "Test")
	if err := s.Reset(); err != nil {
		t.Fatal(err)
	}
	if _, ok := s.Lookup(tok); ok {
		t.Errorf("Reset should have cleared tokens")
	}
}

func TestStore_List(t *testing.T) {
	dir := t.TempDir()
	s, _ := NewStore(filepath.Join(dir, "pairings.json"))
	_, _ = s.Add("a", "A")
	_, _ = s.Add("b", "B")
	pairs := s.List()
	if len(pairs) != 2 {
		t.Errorf("List returned %d, want 2", len(pairs))
	}
}
