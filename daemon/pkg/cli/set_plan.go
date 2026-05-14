package cli

import (
	"fmt"
	"io"

	"github.com/krizdingus/claudometer/daemon/pkg/config"
)

// SetPlanWith updates plan_tier in the config file at path and calls
// restarter (which is expected to restart the daemon process so the new
// plan takes effect). Returns an exit code.
func SetPlanWith(out io.Writer, path, plan string, restarter func() error) int {
	if !validPlan(plan) {
		fmt.Fprintf(out, "error: invalid plan %q (must be free, pro, max-5x, or max-20x)\n", plan)
		return 1
	}
	s, err := config.Load(path)
	if err != nil {
		fmt.Fprintln(out, "error:", err)
		return 1
	}
	s.PlanTier = plan
	if err := config.Save(path, s); err != nil {
		fmt.Fprintln(out, "error:", err)
		return 1
	}
	fmt.Fprintf(out, "Updated plan tier to %s in %s.\n", plan, path)
	if restarter != nil {
		if err := restarter(); err != nil {
			fmt.Fprintf(out, "warning: failed to restart service: %v\n  Run 'brew services restart claudometer' to apply.\n", err)
		} else {
			fmt.Fprintln(out, "Restarted claudometer service.")
		}
	}
	return 0
}

func validPlan(p string) bool {
	switch p {
	case "free", "pro", "max-5x", "max-20x":
		return true
	}
	return false
}
