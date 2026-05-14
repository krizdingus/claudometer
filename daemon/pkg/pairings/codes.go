package pairings

import (
	"crypto/rand"
	"encoding/binary"
	"fmt"
	"sync"
	"time"
)

type PendingRequest struct {
	CydID     string
	Code      string
	ExpiresAt time.Time
}

type Codes struct {
	TTL time.Duration
	Now func() time.Time

	mu      sync.Mutex
	current *PendingRequest
}

func NewCodes(ttl time.Duration) *Codes {
	return &Codes{TTL: ttl, Now: time.Now}
}

func (c *Codes) Request(cydID string) (PendingRequest, error) {
	c.mu.Lock()
	defer c.mu.Unlock()
	var buf [4]byte
	if _, err := rand.Read(buf[:]); err != nil {
		return PendingRequest{}, err
	}
	n := binary.BigEndian.Uint32(buf[:]) % 10000
	code := fmt.Sprintf("%04d", n)
	c.current = &PendingRequest{
		CydID:     cydID,
		Code:      code,
		ExpiresAt: c.Now().Add(c.TTL),
	}
	return *c.current, nil
}

func (c *Codes) Verify(code string) (string, bool) {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.current == nil {
		return "", false
	}
	if c.Now().After(c.current.ExpiresAt) {
		c.current = nil
		return "", false
	}
	if c.current.Code != code {
		return "", false
	}
	cydID := c.current.CydID
	c.current = nil
	return cydID, true
}

func (c *Codes) Pending() string {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.current == nil || c.Now().After(c.current.ExpiresAt) {
		return ""
	}
	return c.current.Code
}
