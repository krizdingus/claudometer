package cli_test

import (
	"bytes"
	"encoding/json"
	"errors"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"github.com/krizdingus/claudometer/daemon/pkg/cli"
	"github.com/krizdingus/claudometer/daemon/pkg/flasher"
	"github.com/krizdingus/claudometer/daemon/pkg/provisioner"
)

type stubServices struct {
	probe       func(port string, timeout time.Duration) (string, bool, error)
	download    func(version, cacheDir string) (flasher.FirmwareBundle, error)
	flash       func(port string, b flasher.FirmwareBundle, progress func(flasher.ProgressEvent)) error
	provision   func(port string, creds provisioner.Creds, timeout time.Duration) error
	waitForPoll func(token string, timeout time.Duration) error
}

func (s *stubServices) Probe(port string, timeout time.Duration) (string, bool, error) {
	return s.probe(port, timeout)
}
func (s *stubServices) Download(v, c string) (flasher.FirmwareBundle, error) { return s.download(v, c) }
func (s *stubServices) Flash(p string, b flasher.FirmwareBundle, cb func(flasher.ProgressEvent)) error {
	return s.flash(p, b, cb)
}
func (s *stubServices) Provision(p string, c provisioner.Creds, t time.Duration) error {
	return s.provision(p, c, t)
}
func (s *stubServices) WaitForFirstPoll(token string, t time.Duration) error {
	return s.waitForPoll(token, t)
}

func TestAddDevice_HappyPathSkipsFlashOnReady(t *testing.T) {
	pairSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		var req map[string]string
		json.NewDecoder(r.Body).Decode(&req)
		if req["cyd_id"] != "AA:BB:CC:DD:EE:FF" {
			t.Errorf("cyd_id = %q", req["cyd_id"])
		}
		w.Write([]byte(`{"token":"deadbeef"}`))
	}))
	defer pairSrv.Close()

	svcs := &stubServices{
		probe: func(port string, timeout time.Duration) (string, bool, error) {
			return "AA:BB:CC:DD:EE:FF", true, nil
		},
		download: func(v, c string) (flasher.FirmwareBundle, error) {
			t.Fatal("download must not be called when probe found READY")
			return flasher.FirmwareBundle{}, nil
		},
		flash: func(p string, b flasher.FirmwareBundle, cb func(flasher.ProgressEvent)) error {
			t.Fatal("flash must not be called when probe found READY")
			return nil
		},
		provision: func(p string, c provisioner.Creds, _ time.Duration) error {
			if c.WifiSSID != "MyWifi" {
				t.Errorf("ssid = %q", c.WifiSSID)
			}
			if c.BearerToken != "deadbeef" {
				t.Errorf("token = %q", c.BearerToken)
			}
			return nil
		},
		waitForPoll: func(token string, _ time.Duration) error { return nil },
	}

	out := &bytes.Buffer{}
	err := cli.AddDeviceWith(svcs, cli.AddDeviceOptions{
		Port:         "/dev/cu.usbserial-1",
		WifiSSID:     "MyWifi",
		WifiPass:     "secret",
		Name:         "",
		AdminPairURL: pairSrv.URL,
		ServerHost:   "192.168.1.42",
		ServerPort:   7842,
		CacheDir:     "/tmp",
		Out:          out,
	})
	if err != nil {
		t.Fatalf("AddDeviceWith: %v", err)
	}
	if !strings.Contains(out.String(), "connected") {
		t.Errorf("output missing success line: %s", out.String())
	}
}

func TestAddDevice_FreshChipFlashesThenProvisions(t *testing.T) {
	pairSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte(`{"token":"tok"}`))
	}))
	defer pairSrv.Close()

	var flashedCount int
	var probeCount int
	svcs := &stubServices{
		probe: func(port string, timeout time.Duration) (string, bool, error) {
			probeCount++
			if probeCount == 1 {
				return "", false, nil
			}
			return "AA:BB:CC:DD:EE:01", true, nil
		},
		download: func(v, c string) (flasher.FirmwareBundle, error) {
			return flasher.FirmwareBundle{Bootloader: "/b", Partitions: "/p", Firmware: "/f"}, nil
		},
		flash: func(p string, b flasher.FirmwareBundle, cb func(flasher.ProgressEvent)) error {
			flashedCount++
			return nil
		},
		provision: func(p string, c provisioner.Creds, _ time.Duration) error {
			if c.BearerToken != "tok" {
				t.Errorf("token = %q", c.BearerToken)
			}
			return nil
		},
		waitForPoll: func(token string, _ time.Duration) error { return nil },
	}

	out := &bytes.Buffer{}
	err := cli.AddDeviceWith(svcs, cli.AddDeviceOptions{
		Port:         "/dev/cu.usbserial-1",
		WifiSSID:     "x",
		WifiPass:     "y",
		AdminPairURL: pairSrv.URL,
		ServerHost:   "h",
		ServerPort:   7842,
		CacheDir:     "/tmp",
		Out:          out,
	})
	if err != nil {
		t.Fatalf("AddDeviceWith: %v", err)
	}
	if flashedCount != 1 {
		t.Errorf("flashedCount = %d, want 1", flashedCount)
	}
}

func TestAddDevice_AdminPairFailureAborts(t *testing.T) {
	pairSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusForbidden)
	}))
	defer pairSrv.Close()

	svcs := &stubServices{
		probe: func(port string, _ time.Duration) (string, bool, error) {
			return "AA:BB:CC:DD:EE:02", true, nil
		},
		provision: func(p string, c provisioner.Creds, _ time.Duration) error {
			t.Fatal("provision must not be called if pair fails")
			return nil
		},
		waitForPoll: func(string, time.Duration) error { return nil },
	}

	err := cli.AddDeviceWith(svcs, cli.AddDeviceOptions{
		Port:         "/dev/x", WifiSSID: "x", WifiPass: "y",
		AdminPairURL: pairSrv.URL, ServerHost: "h", ServerPort: 1, CacheDir: "/tmp",
		Out: &bytes.Buffer{},
	})
	if err == nil || !strings.Contains(err.Error(), "pair") {
		t.Errorf("err = %v, want pair-related error", err)
	}
}

func TestAddDevice_PostProvisionPollTimeoutIsPartialSuccess(t *testing.T) {
	pairSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte(`{"token":"tok"}`))
	}))
	defer pairSrv.Close()

	svcs := &stubServices{
		probe:     func(string, time.Duration) (string, bool, error) { return "AA:BB:CC:DD:EE:04", true, nil },
		provision: func(string, provisioner.Creds, time.Duration) error { return nil },
		waitForPoll: func(string, time.Duration) error {
			return errors.New("timeout waiting for first poll")
		},
	}
	err := cli.AddDeviceWith(svcs, cli.AddDeviceOptions{
		Port: "/dev/x", WifiSSID: "x", WifiPass: "y",
		AdminPairURL: pairSrv.URL, ServerHost: "h", ServerPort: 1, CacheDir: "/tmp",
		Out: &bytes.Buffer{},
	})
	if err == nil {
		t.Fatal("expected partial-success error")
	}
	if !strings.Contains(err.Error(), "didn't connect") {
		t.Errorf("error message should mention 'didn't connect'; got %v", err)
	}
}

func TestDefaultDeviceName_DerivesFromMAC(t *testing.T) {
	got := cli.DefaultDeviceName("AA:BB:CC:DD:EE:FF")
	if got != "cyd-ddeeff" {
		t.Errorf("DefaultDeviceName = %q, want cyd-ddeeff", got)
	}
}

func TestAddDevice_WritesPlanToConfig(t *testing.T) {
	pairSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte(`{"token":"tok"}`))
	}))
	defer pairSrv.Close()

	cfgDir := t.TempDir()
	cfgPath := filepath.Join(cfgDir, "config.json")
	os.WriteFile(cfgPath, []byte(`{"plan_tier":"free","listen_addr":"0.0.0.0:7842"}`), 0o600)

	var restartCalls int
	svcs := &stubServices{
		probe:       func(string, time.Duration) (string, bool, error) { return "AA:BB:CC:DD:EE:99", true, nil },
		provision:   func(string, provisioner.Creds, time.Duration) error { return nil },
		waitForPoll: func(string, time.Duration) error { return nil },
	}

	out := &bytes.Buffer{}
	err := cli.AddDeviceWith(svcs, cli.AddDeviceOptions{
		Port:         "/dev/x",
		WifiSSID:     "x",
		WifiPass:     "y",
		Plan:         "max-5x",
		AdminPairURL: pairSrv.URL,
		ServerHost:   "h",
		ServerPort:   7842,
		CacheDir:     "/tmp",
		ConfigPath:   cfgPath,
		ServiceRestarter: func() error {
			restartCalls++
			return nil
		},
		Out: out,
	})
	if err != nil {
		t.Fatalf("AddDeviceWith: %v", err)
	}

	data, _ := os.ReadFile(cfgPath)
	if !strings.Contains(string(data), `"plan_tier": "max-5x"`) {
		t.Errorf("config not updated; got: %s", data)
	}
	if restartCalls != 1 {
		t.Errorf("restartCalls = %d, want 1", restartCalls)
	}
}

func TestAddDevice_EmptyPlanLeavesConfigAlone(t *testing.T) {
	pairSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte(`{"token":"tok"}`))
	}))
	defer pairSrv.Close()

	cfgDir := t.TempDir()
	cfgPath := filepath.Join(cfgDir, "config.json")
	original := `{"plan_tier":"free","listen_addr":"0.0.0.0:7842"}`
	os.WriteFile(cfgPath, []byte(original), 0o600)

	svcs := &stubServices{
		probe:       func(string, time.Duration) (string, bool, error) { return "AA:BB:CC:DD:EE:88", true, nil },
		provision:   func(string, provisioner.Creds, time.Duration) error { return nil },
		waitForPoll: func(string, time.Duration) error { return nil },
	}

	err := cli.AddDeviceWith(svcs, cli.AddDeviceOptions{
		Port: "/dev/x", WifiSSID: "x", WifiPass: "y",
		AdminPairURL: pairSrv.URL, ServerHost: "h", ServerPort: 7842, CacheDir: "/tmp",
		ConfigPath: cfgPath,
		Out:        &bytes.Buffer{},
	})
	if err != nil {
		t.Fatalf("AddDeviceWith: %v", err)
	}
	data, _ := os.ReadFile(cfgPath)
	if !strings.Contains(string(data), `"plan_tier":"free"`) {
		t.Errorf("config was modified despite empty Plan; got: %s", data)
	}
}
