package cli

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
)

func Status(w io.Writer, baseURL string) int {
	resp, err := http.Get(baseURL + "/v1/status")
	if err != nil {
		fmt.Fprintf(os.Stderr, "daemon not reachable at %s: %v\n", baseURL, err)
		return 1
	}
	defer resp.Body.Close()
	var s struct {
		Version     string `json:"version"`
		PairedCount int    `json:"paired_count"`
		PendingCode string `json:"pending_code"`
	}
	if err := json.NewDecoder(resp.Body).Decode(&s); err != nil {
		fmt.Fprintf(os.Stderr, "bad response: %v\n", err)
		return 1
	}
	fmt.Fprintf(w, "✓ Daemon running (version %s)\n", s.Version)
	fmt.Fprintf(w, "  Paired CYDs: %d\n", s.PairedCount)
	if s.PendingCode != "" {
		fmt.Fprintf(w, "  Pairing code: %s\n", s.PendingCode)
		fmt.Fprintf(w, "  Type this code on your CYD to confirm.\n")
	}
	return 0
}
