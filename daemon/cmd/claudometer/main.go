package main

import (
	"bufio"
	"context"
	"flag"
	"fmt"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"

	"github.com/krizdingus/claudometer/daemon/pkg/claudedata"
	"github.com/krizdingus/claudometer/daemon/pkg/cli"
	"github.com/krizdingus/claudometer/daemon/pkg/config"
	"github.com/krizdingus/claudometer/daemon/pkg/provisioner"
	"github.com/krizdingus/claudometer/daemon/pkg/runner"
)

const (
	version    = "0.1.2"
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
		case "add-device":
			os.Exit(runAddDevice(os.Args[2:]))
		case "set-plan":
			os.Exit(runSetPlan(os.Args[2:]))
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

func runAddDevice(args []string) int {
	fs := flag.NewFlagSet("add-device", flag.ExitOnError)
	port := fs.String("port", "", "serial port path (default: auto-detect)")
	ssid := fs.String("ssid", "", "WiFi SSID")
	password := fs.String("password", "", "WiFi password (use $CLAUDOMETER_WIFI_PASSWORD env var to avoid shell history)")
	name := fs.String("name", "", "pairing label (default: cyd-<mac-suffix>)")
	firmwareDir := fs.String("firmware", "", "local directory containing bootloader.bin/partitions.bin/firmware.bin")
	firmwareVersion := fs.String("firmware-version", "", "release version to download (default: latest)")
	noFlash := fs.Bool("no-flash", false, "fail if firmware not present (don't auto-flash)")
	reflash := fs.Bool("reflash", false, "flash even if firmware already present")
	plan := fs.String("plan", "", "Claude plan tier: free | pro | max-5x | max-20x (default: prompt)")
	if err := fs.Parse(args); err != nil {
		return 2
	}

	if *password == "" {
		*password = os.Getenv("CLAUDOMETER_WIFI_PASSWORD")
	}

	resolvedPort, err := resolvePort(*port)
	if err != nil {
		fmt.Fprintln(os.Stderr, "error:", err)
		return 1
	}

	var resolvedSSID, resolvedPass, resolvedPlan, configPath string
	if !*reflash {
		resolvedSSID, resolvedPass, err = promptWifiIfNeeded(*ssid, *password)
		if err != nil {
			fmt.Fprintln(os.Stderr, "error:", err)
			return 1
		}
		resolvedPlan, err = promptPlanIfNeeded(*plan)
		if err != nil {
			fmt.Fprintln(os.Stderr, "error:", err)
			return 1
		}
		configPath, _ = config.DefaultPath()
	}

	home, _ := os.UserHomeDir()
	cacheDir := filepath.Join(home, ".cache", "claudometer", "firmware")
	listenHost, listenPort := splitListenAddr()

	err = cli.AddDevice(cli.AddDeviceOptions{
		Port:             resolvedPort,
		WifiSSID:         resolvedSSID,
		WifiPass:         resolvedPass,
		Name:             *name,
		FirmwareDir:      *firmwareDir,
		FirmwareVersion:  *firmwareVersion,
		NoFlash:          *noFlash,
		Reflash:          *reflash,
		AdminPairURL:     fmt.Sprintf("http://127.0.0.1:%d/v1/admin/pair", listenPort),
		ServerHost:       listenHost,
		ServerPort:       listenPort,
		CacheDir:         cacheDir,
		Out:              os.Stdout,
		Plan:             resolvedPlan,
		ConfigPath:       configPath,
		ServiceRestarter: brewRestartClaudometer,
	})
	if err != nil {
		fmt.Fprintln(os.Stderr, "error:", err)
		return 1
	}
	return 0
}

func resolvePort(explicit string) (string, error) {
	if explicit != "" {
		return explicit, nil
	}
	ports, err := provisioner.EnumeratePorts()
	if err != nil {
		return "", err
	}
	switch len(ports) {
	case 0:
		return "", fmt.Errorf("no USB-serial ports detected; plug in your CYD and try again")
	case 1:
		fmt.Printf("Using %s\n", ports[0].Name)
		return ports[0].Name, nil
	default:
		fmt.Println("Multiple serial ports detected:")
		for i, p := range ports {
			fmt.Printf("  [%d] %s\n", i+1, p.Name)
		}
		fmt.Print("Pick one (number): ")
		reader := bufio.NewReader(os.Stdin)
		line, _ := reader.ReadString('\n')
		idx, err := strconv.Atoi(strings.TrimSpace(line))
		if err != nil || idx < 1 || idx > len(ports) {
			return "", fmt.Errorf("invalid selection")
		}
		return ports[idx-1].Name, nil
	}
}

func runSetPlan(args []string) int {
	if len(args) == 0 || strings.HasPrefix(args[0], "-") {
		fmt.Fprintln(os.Stderr, "usage: claudometer set-plan <free|pro|max-5x|max-20x>")
		return 2
	}
	path, err := config.DefaultPath()
	if err != nil {
		fmt.Fprintln(os.Stderr, "error:", err)
		return 1
	}
	return cli.SetPlanWith(os.Stdout, path, args[0], brewRestartClaudometer)
}

func promptPlanIfNeeded(supplied string) (string, error) {
	if supplied != "" {
		if !isValidPlan(supplied) {
			return "", fmt.Errorf("invalid --plan %q (must be free, pro, max-5x, or max-20x)", supplied)
		}
		return supplied, nil
	}
	fmt.Println("Plan tier (free | pro | max-5x | max-20x) [free]:")
	reader := bufio.NewReader(os.Stdin)
	line, err := reader.ReadString('\n')
	if err != nil {
		return "", err
	}
	plan := strings.TrimSpace(line)
	if plan == "" {
		plan = "free"
	}
	if !isValidPlan(plan) {
		return "", fmt.Errorf("invalid plan %q (must be free, pro, max-5x, or max-20x)", plan)
	}
	return plan, nil
}

func isValidPlan(p string) bool {
	switch p {
	case "free", "pro", "max-5x", "max-20x":
		return true
	}
	return false
}

func brewRestartClaudometer() error {
	if _, err := exec.LookPath("brew"); err != nil {
		return err
	}
	out, err := exec.Command("brew", "services", "restart", "claudometer").CombinedOutput()
	if err != nil {
		return fmt.Errorf("%w: %s", err, strings.TrimSpace(string(out)))
	}
	return nil
}

func promptWifiIfNeeded(ssid, password string) (string, string, error) {
	reader := bufio.NewReader(os.Stdin)
	if ssid == "" {
		fmt.Print("WiFi SSID: ")
		line, err := reader.ReadString('\n')
		if err != nil {
			return "", "", err
		}
		ssid = strings.TrimSpace(line)
	}
	if password == "" {
		fmt.Print("WiFi password: ")
		line, err := reader.ReadString('\n')
		if err != nil {
			return "", "", err
		}
		password = strings.TrimSpace(line)
	}
	if ssid == "" || password == "" {
		return "", "", fmt.Errorf("both SSID and password are required")
	}
	return ssid, password, nil
}

func splitListenAddr() (string, int) {
	// The firmware needs a host the CYD can actually reach over the LAN,
	// so we use <hostname>.local rather than 0.0.0.0 from cfg.ListenAddr.
	// Port is conventionally 7842; if the user overrides cfg.ListenAddr,
	// they'll need a future --port-listen flag for v1.
	host, _ := os.Hostname()
	host = host + ".local"
	return host, 7842
}
