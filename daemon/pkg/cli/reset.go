package cli

import (
	"fmt"
	"io"

	"github.com/krizdingus/cydmonitor/daemon/pkg/pairings"
)

func ResetPairings(w io.Writer, path string) int {
	store, err := pairings.NewStore(path)
	if err != nil {
		fmt.Fprintln(w, "error opening store:", err)
		return 1
	}
	count := len(store.List())
	if err := store.Reset(); err != nil {
		fmt.Fprintln(w, "reset failed:", err)
		return 1
	}
	fmt.Fprintf(w, "Removed %d paired CYD(s)\n", count)
	return 0
}
