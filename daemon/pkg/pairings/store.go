package pairings

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"errors"
	"io/fs"
	"os"
	"path/filepath"
	"sync"
	"time"
)

type Pair struct {
	Token     string    `json:"token"`
	CydID     string    `json:"cyd_id"`
	Name      string    `json:"name"`
	CreatedAt time.Time `json:"created_at"`
}

type Store struct {
	path string
	mu   sync.Mutex
	pairs map[string]Pair // token → pair
}

func NewStore(path string) (*Store, error) {
	s := &Store{path: path, pairs: map[string]Pair{}}
	if err := s.load(); err != nil {
		return nil, err
	}
	return s, nil
}

func (s *Store) load() error {
	data, err := os.ReadFile(s.path)
	if errors.Is(err, fs.ErrNotExist) {
		return nil
	}
	if err != nil {
		return err
	}
	var list []Pair
	if err := json.Unmarshal(data, &list); err != nil {
		return err
	}
	for _, p := range list {
		s.pairs[p.Token] = p
	}
	return nil
}

func (s *Store) save() error {
	if err := os.MkdirAll(filepath.Dir(s.path), 0o755); err != nil {
		return err
	}
	list := make([]Pair, 0, len(s.pairs))
	for _, p := range s.pairs {
		list = append(list, p)
	}
	data, err := json.MarshalIndent(list, "", "  ")
	if err != nil {
		return err
	}
	return os.WriteFile(s.path, data, 0o600)
}

func (s *Store) Add(cydID, name string) (string, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	buf := make([]byte, 32)
	if _, err := rand.Read(buf); err != nil {
		return "", err
	}
	token := hex.EncodeToString(buf)
	s.pairs[token] = Pair{Token: token, CydID: cydID, Name: name, CreatedAt: time.Now()}
	return token, s.save()
}

func (s *Store) Lookup(token string) (Pair, bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	p, ok := s.pairs[token]
	return p, ok
}

func (s *Store) List() []Pair {
	s.mu.Lock()
	defer s.mu.Unlock()
	out := make([]Pair, 0, len(s.pairs))
	for _, p := range s.pairs {
		out = append(out, p)
	}
	return out
}

func (s *Store) Reset() error {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.pairs = map[string]Pair{}
	return s.save()
}
