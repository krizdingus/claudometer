package main

import (
	"context"
	"fmt"
	"os"
	"os/signal"
	"path/filepath"
	"syscall"

	"github.com/krizdingus/claudometer/daemon/pkg/claudedata"
	"github.com/krizdingus/claudometer/daemon/pkg/cli"
	"github.com/krizdingus/claudometer/daemon/pkg/config"
	"github.com/krizdingus/claudometer/daemon/pkg/runner"
)

const (
	version    = "0.1.0-dev"
	listenAddr = "0.0.0.0:7842"
	statusURL  = "http://127.0.0.1:7842"
)

func main() {
	if len(os.Args) > 1 {
		switch os.Args[1] {
		case "version":
			fmt.Println(version)
			return
		case "status":
			os.Exit(cli.Status(os.Stdout, statusURL))
		case "reset-pairings":
			os.Exit(cli.ResetPairings(os.Stdout, pairingsPath()))
		}
	}
	if err := serve(); err != nil {
		fmt.Fprintln(os.Stderr, "fatal:", err)
		os.Exit(1)
	}
}

func serve() error {
	home, err := os.UserHomeDir()
	if err != nil {
		return err
	}

	configPath, err := config.DefaultPath()
	if err != nil {
		return err
	}
	cfg, err := config.EnsureExists(configPath)
	if err != nil {
		return fmt.Errorf("config: %w", err)
	}

	// One-shot migration: copy the old cydmonitor pairings file to the new
	// claudometer path if the user is upgrading. Silently no-ops otherwise.
	migratePairings(home)

	claudeDir := filepath.Join(home, ".claude")
	planInfo, _ := claudedata.ReadPlanInfo(filepath.Join(claudeDir, ".claude.json"))
	if cfg.PlanTier != "" {
		planInfo.Plan = claudedata.Plan(cfg.PlanTier)
	}

	svc, err := runner.New(runner.Options{
		ListenAddr:   cfg.ListenAddr,
		ProjectsDir:  filepath.Join(claudeDir, "projects"),
		PairingsPath: pairingsPath(),
		PlanInfo:     planInfo,
		Caps:         cfg.Caps(),
		Version:      version,
		Logger:       func(f string, a ...any) { fmt.Fprintf(os.Stderr, f+"\n", a...) },
	})
	if err != nil {
		return err
	}

	ctx, cancel := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer cancel()
	return svc.Start(ctx)
}

func pairingsPath() string {
	home, _ := os.UserHomeDir()
	return filepath.Join(home, ".config", "claudometer", "pairings.json")
}

// migratePairings copies ~/.config/cydmonitor/pairings.json to
// ~/.config/claudometer/pairings.json if the new path is missing but the old
// path exists. After copying it leaves the old file in place so users can
// downgrade if needed.
func migratePairings(home string) {
	newPath := filepath.Join(home, ".config", "claudometer", "pairings.json")
	oldPath := filepath.Join(home, ".config", "cydmonitor", "pairings.json")
	if _, err := os.Stat(newPath); err == nil {
		return
	}
	if _, err := os.Stat(oldPath); err != nil {
		return
	}
	data, err := os.ReadFile(oldPath)
	if err != nil {
		return
	}
	if err := os.MkdirAll(filepath.Dir(newPath), 0o755); err != nil {
		return
	}
	_ = os.WriteFile(newPath, data, 0o600)
}
