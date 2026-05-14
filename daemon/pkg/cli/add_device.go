package cli

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	"github.com/krizdingus/claudometer/daemon/pkg/config"
	"github.com/krizdingus/claudometer/daemon/pkg/flasher"
	"github.com/krizdingus/claudometer/daemon/pkg/provisioner"
)

// AddDeviceOptions configures one invocation of AddDevice.
type AddDeviceOptions struct {
	Port             string
	WifiSSID         string
	WifiPass         string
	Name             string
	FirmwareDir      string // if non-empty, use LocalBundle instead of Download
	FirmwareVersion  string // empty = "latest"
	NoFlash          bool   // force skip flashing even if probe didn't see READY
	Reflash          bool   // force flash even if probe DID see READY
	AdminPairURL     string // e.g. http://127.0.0.1:7842/v1/admin/pair
	ServerHost       string
	ServerPort       int
	CacheDir         string
	Out              io.Writer
	ProbeTimeout     time.Duration
	ProvisionTimeout time.Duration
	PollTimeout      time.Duration

	// Plan, if non-empty, is written into the config file's plan_tier field
	// after successful pairing. Valid values: "free", "pro", "max-5x", "max-20x".
	Plan string
	// ConfigPath is where to write the plan_tier update. Empty = skip the write.
	ConfigPath string
	// ServiceRestarter is called after the config write to restart the
	// background daemon so it picks up the new plan. nil = skip restart.
	ServiceRestarter func() error
}

// Services is the test seam for AddDevice.
type Services interface {
	Probe(port string, timeout time.Duration) (string, bool, error)
	Download(version, cacheDir string) (flasher.FirmwareBundle, error)
	Flash(port string, b flasher.FirmwareBundle, progress func(flasher.ProgressEvent)) error
	Provision(port string, c provisioner.Creds, timeout time.Duration) error
	WaitForFirstPoll(token string, timeout time.Duration) error
}

// AddDevice is the production entrypoint wired against the real services.
func AddDevice(opts AddDeviceOptions) error {
	return AddDeviceWith(realServices{}, opts)
}

// AddDeviceWith is the test-injectable variant.
func AddDeviceWith(svcs Services, opts AddDeviceOptions) error {
	if opts.Out == nil {
		opts.Out = io.Discard
	}
	if opts.ProbeTimeout == 0 {
		opts.ProbeTimeout = 5 * time.Second
	}
	if opts.ProvisionTimeout == 0 {
		opts.ProvisionTimeout = 30 * time.Second
	}
	if opts.PollTimeout == 0 {
		opts.PollTimeout = 60 * time.Second
	}

	if opts.Reflash {
		fmt.Fprintln(opts.Out, "Flashing firmware (preserving existing pairing)...")
		bundle, err := loadBundle(svcs, opts)
		if err != nil {
			return err
		}
		progress := func(ev flasher.ProgressEvent) {
			fmt.Fprintf(opts.Out, "  %s: %d%%\n", ev.Offset, ev.Pct)
		}
		if err := svcs.Flash(opts.Port, bundle, progress); err != nil {
			return fmt.Errorf("flash: %w", err)
		}
		fmt.Fprintln(opts.Out, "Firmware updated; existing pairing preserved.")
		return nil
	}

	mac, found, err := svcs.Probe(opts.Port, opts.ProbeTimeout)
	if err != nil {
		return fmt.Errorf("probe %s: %w", opts.Port, err)
	}

	if !found && opts.NoFlash {
		return fmt.Errorf("no READY seen on %s and --no-flash set; remove --no-flash or run without it", opts.Port)
	}

	if !found {
		fmt.Fprintln(opts.Out, "Flashing firmware...")
		bundle, err := loadBundle(svcs, opts)
		if err != nil {
			return err
		}
		progress := func(ev flasher.ProgressEvent) {
			fmt.Fprintf(opts.Out, "  %s: %d%%\n", ev.Offset, ev.Pct)
		}
		if err := svcs.Flash(opts.Port, bundle, progress); err != nil {
			return fmt.Errorf("flash: %w", err)
		}
		mac, found, err = svcs.Probe(opts.Port, 30*time.Second)
		if err != nil || !found {
			return fmt.Errorf("post-flash probe failed (err=%v found=%v)", err, found)
		}
	}

	name := opts.Name
	if name == "" {
		name = DefaultDeviceName(mac)
	}
	token, err := mintToken(opts.AdminPairURL, mac, name)
	if err != nil {
		return fmt.Errorf("pair: %w", err)
	}
	fmt.Fprintf(opts.Out, "Paired as %s (token minted).\n", name)

	creds := provisioner.Creds{
		WifiSSID:     opts.WifiSSID,
		WifiPassword: opts.WifiPass,
		ServerHost:   opts.ServerHost,
		ServerPort:   opts.ServerPort,
		BearerToken:  token,
	}
	if err := svcs.Provision(opts.Port, creds, opts.ProvisionTimeout); err != nil {
		return fmt.Errorf("provision: %w", err)
	}
	fmt.Fprintln(opts.Out, "Provisioning JSON pushed; firmware ACKed OK.")

	fmt.Fprintln(opts.Out, "Waiting for the CYD to come online...")
	if err := svcs.WaitForFirstPoll(token, opts.PollTimeout); err != nil {
		return fmt.Errorf("device didn't connect within %s: %w (check WiFi password and signal)", opts.PollTimeout, err)
	}
	fmt.Fprintf(opts.Out, "%s connected.\n", name)

	if opts.Plan != "" && opts.ConfigPath != "" {
		if err := updatePlanInConfig(opts.ConfigPath, opts.Plan); err != nil {
			fmt.Fprintf(opts.Out, "warning: failed to write plan tier to %s: %v\n", opts.ConfigPath, err)
		} else {
			fmt.Fprintf(opts.Out, "Updated plan tier to %s in %s.\n", opts.Plan, opts.ConfigPath)
			if opts.ServiceRestarter != nil {
				if err := opts.ServiceRestarter(); err != nil {
					fmt.Fprintf(opts.Out, "warning: failed to restart service: %v\n  Run 'brew services restart claudometer' to apply.\n", err)
				} else {
					fmt.Fprintln(opts.Out, "Restarted claudometer service.")
				}
			}
		}
	}
	return nil
}

func loadBundle(svcs Services, opts AddDeviceOptions) (flasher.FirmwareBundle, error) {
	if opts.FirmwareDir != "" {
		b, err := flasher.LocalBundle(opts.FirmwareDir)
		if err != nil {
			return flasher.FirmwareBundle{}, fmt.Errorf("firmware bundle: %w", err)
		}
		return b, nil
	}
	b, err := svcs.Download(opts.FirmwareVersion, opts.CacheDir)
	if err != nil {
		return flasher.FirmwareBundle{}, fmt.Errorf("firmware bundle: %w", err)
	}
	return b, nil
}

func updatePlanInConfig(path, plan string) error {
	s, err := config.Load(path)
	if err != nil {
		return err
	}
	s.PlanTier = plan
	return config.Save(path, s)
}

// DefaultDeviceName derives "cyd-aabbcc" from the last 3 octets of a MAC.
func DefaultDeviceName(mac string) string {
	parts := strings.Split(strings.ReplaceAll(mac, "-", ":"), ":")
	if len(parts) < 6 {
		return "cyd-" + strings.ToLower(strings.NewReplacer(":", "", "-", "").Replace(mac))
	}
	tail := strings.ToLower(parts[3] + parts[4] + parts[5])
	return "cyd-" + tail
}

func mintToken(adminURL, cydID, name string) (string, error) {
	body, _ := json.Marshal(map[string]string{"cyd_id": cydID, "name": name})
	resp, err := http.Post(adminURL, "application/json", bytes.NewReader(body))
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		buf, _ := io.ReadAll(resp.Body)
		return "", fmt.Errorf("admin/pair %d: %s", resp.StatusCode, bytes.TrimSpace(buf))
	}
	var out struct {
		Token string `json:"token"`
	}
	if err := json.NewDecoder(resp.Body).Decode(&out); err != nil {
		return "", err
	}
	if out.Token == "" {
		return "", errors.New("admin/pair returned empty token")
	}
	return out.Token, nil
}

// realServices wires AddDevice against the actual flasher/provisioner.
type realServices struct{}

func (realServices) Probe(port string, t time.Duration) (string, bool, error) {
	return provisioner.Probe(port, t)
}
func (realServices) Download(v, c string) (flasher.FirmwareBundle, error) {
	return flasher.Download(v, c)
}
func (realServices) Flash(p string, b flasher.FirmwareBundle, cb func(flasher.ProgressEvent)) error {
	return flasher.Flash(p, b, cb)
}
func (realServices) Provision(p string, c provisioner.Creds, t time.Duration) error {
	return provisioner.Provision(p, c, t)
}
func (realServices) WaitForFirstPoll(token string, timeout time.Duration) error {
	// Approximate: poll /v1/stats with the new bearer. The daemon answers 200
	// as soon as the bearer is registered (which happens at mintToken time),
	// so this primarily catches "daemon went down" or network-level failures.
	// Refining to track first-poll-time on the daemon is future work.
	deadline := time.Now().Add(timeout)
	url := "http://127.0.0.1:7842/v1/stats"
	for time.Now().Before(deadline) {
		req, _ := http.NewRequest(http.MethodGet, url, nil)
		req.Header.Set("Authorization", "Bearer "+token)
		resp, err := http.DefaultClient.Do(req)
		if err == nil {
			resp.Body.Close()
			if resp.StatusCode == http.StatusOK {
				return nil
			}
		}
		time.Sleep(1 * time.Second)
	}
	return fmt.Errorf("timeout")
}
