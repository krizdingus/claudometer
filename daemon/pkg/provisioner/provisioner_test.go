package provisioner_test

import (
	"bytes"
	"errors"
	"io"
	"strings"
	"testing"
	"time"

	"github.com/krizdingus/claudometer/daemon/pkg/provisioner"
)

// fakePort is a serial-port stand-in that reads from one buffer and writes
// to another. We use it to feed canned READY/OK responses.
type fakePort struct {
	in  *bytes.Buffer
	out *bytes.Buffer
}

func (f *fakePort) Read(p []byte) (int, error) {
	if f.in.Len() == 0 {
		return 0, io.EOF
	}
	return f.in.Read(p)
}

func (f *fakePort) Write(p []byte) (int, error) { return f.out.Write(p) }
func (f *fakePort) Close() error                { return nil }

func TestProbe_ReturnsMacOnReady(t *testing.T) {
	port := &fakePort{
		in:  bytes.NewBufferString("READY AA:BB:CC:DD:EE:FF\n"),
		out: &bytes.Buffer{},
	}
	mac, found, err := provisioner.ProbeReader(port, 100*time.Millisecond)
	if err != nil {
		t.Fatalf("ProbeReader: %v", err)
	}
	if !found {
		t.Fatalf("found = false, want true")
	}
	if mac != "AA:BB:CC:DD:EE:FF" {
		t.Errorf("mac = %q, want AA:BB:CC:DD:EE:FF", mac)
	}
}

func TestProbe_TimesOutWhenNoReady(t *testing.T) {
	port := &fakePort{
		in:  bytes.NewBufferString("blah blah no ready here\n"),
		out: &bytes.Buffer{},
	}
	_, found, err := provisioner.ProbeReader(port, 50*time.Millisecond)
	if err != nil {
		t.Fatalf("ProbeReader: %v", err)
	}
	if found {
		t.Errorf("found = true, want false (no READY in stream)")
	}
}

func TestProvision_SendsJSONAndExpectsOK(t *testing.T) {
	port := &fakePort{
		in:  bytes.NewBufferString("OK\n"),
		out: &bytes.Buffer{},
	}
	err := provisioner.ProvisionReader(port, provisioner.Creds{
		WifiSSID:     "MyWifi",
		WifiPassword: "secret",
		ServerHost:   "192.168.1.42",
		ServerPort:   7842,
		BearerToken:  "abc123",
	}, 100*time.Millisecond)
	if err != nil {
		t.Fatalf("ProvisionReader: %v", err)
	}
	sent := port.out.String()
	if !strings.Contains(sent, `"wifi_ssid":"MyWifi"`) {
		t.Errorf("payload missing wifi_ssid; got %q", sent)
	}
	if !strings.Contains(sent, `"provision_schema":1`) {
		t.Errorf("payload missing provision_schema=1; got %q", sent)
	}
	if !strings.HasSuffix(sent, "\n") {
		t.Errorf("payload not newline-terminated; got %q", sent)
	}
}

func TestProvision_ReturnsErrorOnErrLine(t *testing.T) {
	port := &fakePort{
		in:  bytes.NewBufferString("ERR invalid wifi_password\n"),
		out: &bytes.Buffer{},
	}
	err := provisioner.ProvisionReader(port, provisioner.Creds{
		WifiSSID:     "x",
		WifiPassword: "y",
		ServerHost:   "h",
		ServerPort:   1,
		BearerToken:  "t",
	}, 100*time.Millisecond)
	if err == nil {
		t.Fatal("ProvisionReader: expected error, got nil")
	}
	if !strings.Contains(err.Error(), "invalid wifi_password") {
		t.Errorf("error = %q; want it to contain firmware ERR reason", err)
	}
}

func TestProvision_TimesOutWithoutOK(t *testing.T) {
	port := &fakePort{
		in:  bytes.NewBufferString(""),
		out: &bytes.Buffer{},
	}
	err := provisioner.ProvisionReader(port, provisioner.Creds{
		WifiSSID: "x", WifiPassword: "y", ServerHost: "h", ServerPort: 1, BearerToken: "t",
	}, 50*time.Millisecond)
	if !errors.Is(err, provisioner.ErrAckTimeout) {
		t.Errorf("err = %v, want ErrAckTimeout", err)
	}
}
