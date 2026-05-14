// Package provisioner handles the USB-serial side of CYD onboarding:
// enumerating ports, probing for the firmware's READY signal, and pushing
// provisioning JSON. The package is split into pure ReadWriter-based
// helpers (Reader-suffixed) and platform-talking wrappers that open the
// real serial port via go.bug.st/serial.
package provisioner

import (
	"bufio"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"strings"
	"time"

	"go.bug.st/serial"
)

// Creds is the host-side mirror of the firmware's ProvisioningCreds struct
// (firmware/src/net/usb_provisioner.h). JSON tags must match what the
// firmware parser expects, including provision_schema:1.
type Creds struct {
	WifiSSID     string `json:"wifi_ssid"`
	WifiPassword string `json:"wifi_password"`
	ServerHost   string `json:"server_host"`
	ServerPort   int    `json:"server_port"`
	BearerToken  string `json:"bearer_token"`
}

type credsWithSchema struct {
	Creds
	ProvisionSchema int `json:"provision_schema"`
}

// ErrAckTimeout means provisioning JSON was sent but no OK arrived within
// the deadline.
var ErrAckTimeout = errors.New("provisioner: timed out waiting for OK")

// Port describes a serial device.
type Port struct {
	Name        string
	Description string
}

// EnumeratePorts returns serial ports filtered to common USB-serial naming
// patterns (best-effort cross-platform — manual smoke covers the real
// device side).
func EnumeratePorts() ([]Port, error) {
	names, err := serial.GetPortsList()
	if err != nil {
		return nil, err
	}
	out := make([]Port, 0, len(names))
	for _, n := range names {
		if !looksLikeUSBSerial(n) {
			continue
		}
		out = append(out, Port{Name: n})
	}
	return out, nil
}

func looksLikeUSBSerial(name string) bool {
	lower := strings.ToLower(name)
	return strings.Contains(lower, "usbserial") ||
		strings.Contains(lower, "usbmodem") ||
		strings.Contains(lower, "ttyusb") ||
		strings.Contains(lower, "ttyacm") ||
		strings.Contains(lower, "wchusbserial")
}

// Probe opens the serial port at 115200, watches for "READY <MAC>" for up
// to timeout, and returns the MAC if seen.
func Probe(portName string, timeout time.Duration) (mac string, found bool, err error) {
	port, err := openSerial(portName)
	if err != nil {
		return "", false, err
	}
	defer port.Close()
	return ProbeReader(port, timeout)
}

// ProbeReader is the testable core.
func ProbeReader(r io.Reader, timeout time.Duration) (string, bool, error) {
	deadline := time.Now().Add(timeout)
	scanner := bufio.NewScanner(r)
	for scanner.Scan() {
		if time.Now().After(deadline) {
			return "", false, nil
		}
		line := strings.TrimSpace(scanner.Text())
		if strings.HasPrefix(line, "READY ") {
			return strings.TrimSpace(strings.TrimPrefix(line, "READY ")), true, nil
		}
	}
	if err := scanner.Err(); err != nil && !errors.Is(err, io.EOF) {
		return "", false, err
	}
	return "", false, nil
}

// Provision opens the serial port, sends one JSON line, waits for OK\n.
func Provision(portName string, c Creds, timeout time.Duration) error {
	port, err := openSerial(portName)
	if err != nil {
		return err
	}
	defer port.Close()
	return ProvisionReader(port, c, timeout)
}

// ProvisionReader is the testable core.
func ProvisionReader(rw io.ReadWriter, c Creds, timeout time.Duration) error {
	payload, err := json.Marshal(credsWithSchema{Creds: c, ProvisionSchema: 1})
	if err != nil {
		return fmt.Errorf("marshal: %w", err)
	}
	payload = append(payload, '\n')
	if _, err := rw.Write(payload); err != nil {
		return fmt.Errorf("write: %w", err)
	}

	deadline := time.Now().Add(timeout)
	scanner := bufio.NewScanner(rw)
	for scanner.Scan() {
		if time.Now().After(deadline) {
			return ErrAckTimeout
		}
		line := strings.TrimSpace(scanner.Text())
		switch {
		case line == "OK":
			return nil
		case strings.HasPrefix(line, "ERR "):
			return fmt.Errorf("firmware: %s", strings.TrimPrefix(line, "ERR "))
		}
	}
	if err := scanner.Err(); err != nil && !errors.Is(err, io.EOF) {
		return err
	}
	return ErrAckTimeout
}

func openSerial(name string) (serial.Port, error) {
	mode := &serial.Mode{BaudRate: 115200}
	port, err := serial.Open(name, mode)
	if err != nil {
		return nil, fmt.Errorf("open %s: %w", name, err)
	}
	if err := port.SetReadTimeout(100 * time.Millisecond); err != nil {
		port.Close()
		return nil, fmt.Errorf("set read timeout: %w", err)
	}
	return port, nil
}
