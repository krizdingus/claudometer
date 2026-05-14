package server

import (
	"net/http"
	"strings"

	"github.com/krizdingus/claudometer/daemon/pkg/pairings"
)

// RequireToken wraps a handler to enforce a valid Bearer token in the
// Authorization header. Tokens are looked up in the pairings store.
func RequireToken(store *pairings.Store, next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		header := r.Header.Get("Authorization")
		token, ok := strings.CutPrefix(header, "Bearer ")
		if !ok || token == "" {
			http.Error(w, "missing or malformed Authorization header", http.StatusUnauthorized)
			return
		}
		if _, found := store.Lookup(token); !found {
			http.Error(w, "invalid token", http.StatusUnauthorized)
			return
		}
		next.ServeHTTP(w, r)
	})
}
