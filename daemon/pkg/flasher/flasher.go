// Package flasher manages firmware delivery + the esptool subprocess for
// the add-device subcommand. Download fetches the three ESP32 artifacts
// from a GitHub Release and caches them. Flash shells out to esptool with
// the standard ESP32 offsets. Local-bundle and runner-injection seams
// keep everything unit-testable.
package flasher

import (
	"fmt"
	"io"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
)

const (
	defaultBaseURL = "https://github.com/krizdingus/claudometer/releases/download"
	bootloaderName = "bootloader.bin"
	partitionsName = "partitions.bin"
	firmwareName   = "firmware.bin"
)

// FirmwareBundle is a set of three absolute paths pointing at the .bin
// files esptool needs to flash an ESP32-2432S028.
type FirmwareBundle struct {
	Bootloader string
	Partitions string
	Firmware   string
}

// Download fetches the latest released firmware. Pass an empty version
// string to use "latest".
func Download(version, cacheDir string) (FirmwareBundle, error) {
	return DownloadFromBase(defaultBaseURL, version, cacheDir)
}

// DownloadFromBase exposes the base URL for tests.
func DownloadFromBase(baseURL, version, cacheDir string) (FirmwareBundle, error) {
	if version == "" {
		version = "latest"
	}
	dir := filepath.Join(cacheDir, version)
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return FirmwareBundle{}, err
	}
	bundle := FirmwareBundle{
		Bootloader: filepath.Join(dir, bootloaderName),
		Partitions: filepath.Join(dir, partitionsName),
		Firmware:   filepath.Join(dir, firmwareName),
	}
	if isCached(bundle) {
		return bundle, nil
	}
	for _, name := range []string{bootloaderName, partitionsName, firmwareName} {
		url := fmt.Sprintf("%s/%s/%s", strings.TrimRight(baseURL, "/"), version, name)
		dest := filepath.Join(dir, name)
		if err := downloadFile(url, dest); err != nil {
			return FirmwareBundle{}, fmt.Errorf("download %s: %w", name, err)
		}
	}
	return bundle, nil
}

func isCached(b FirmwareBundle) bool {
	for _, p := range []string{b.Bootloader, b.Partitions, b.Firmware} {
		fi, err := os.Stat(p)
		if err != nil || fi.Size() == 0 {
			return false
		}
	}
	return true
}

func downloadFile(url, dest string) error {
	resp, err := http.Get(url)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("http %d for %s", resp.StatusCode, url)
	}
	tmp := dest + ".tmp"
	f, err := os.Create(tmp)
	if err != nil {
		return err
	}
	if _, err := io.Copy(f, resp.Body); err != nil {
		f.Close()
		return err
	}
	if err := f.Close(); err != nil {
		return err
	}
	return os.Rename(tmp, dest)
}

// LocalBundle reads three .bin files from a user-specified directory.
func LocalBundle(dir string) (FirmwareBundle, error) {
	b := FirmwareBundle{
		Bootloader: filepath.Join(dir, bootloaderName),
		Partitions: filepath.Join(dir, partitionsName),
		Firmware:   filepath.Join(dir, firmwareName),
	}
	for _, p := range []string{b.Bootloader, b.Partitions, b.Firmware} {
		if _, err := os.Stat(p); err != nil {
			return FirmwareBundle{}, fmt.Errorf("missing %s: %w", filepath.Base(p), err)
		}
	}
	return b, nil
}

// ProgressEvent is what the Flash callback receives.
type ProgressEvent struct {
	Offset string
	Pct    int
}

// Runner is the test seam for esptool invocation.
type Runner func(name string, args ...string) ([]byte, error)

// Flash runs esptool with the standard ESP32 offsets.
func Flash(port string, b FirmwareBundle, progress func(ProgressEvent)) error {
	return FlashWithRunner(port, b, defaultRunner, progress)
}

// FlashWithRunner is the test-injectable variant.
func FlashWithRunner(port string, b FirmwareBundle, runner Runner, progress func(ProgressEvent)) error {
	args := []string{
		"--port", port,
		"--baud", "460800",
		"write_flash",
		"0x1000", b.Bootloader,
		"0x8000", b.Partitions,
		"0x10000", b.Firmware,
	}
	name := pickEsptool()
	out, err := runner(name, args...)
	combined := string(out)
	if progress != nil {
		ParseProgress(combined, progress)
	}
	if err != nil {
		return fmt.Errorf("esptool: %w: %s", err, extractError(combined))
	}
	return nil
}

func defaultRunner(name string, args ...string) ([]byte, error) {
	return exec.Command(name, args...).CombinedOutput()
}

func pickEsptool() string {
	if _, err := exec.LookPath("esptool"); err == nil {
		return "esptool"
	}
	return "esptool.py"
}

// progressLine matches lines like "Writing at 0x00010000... (40 %)"
var progressLine = regexp.MustCompile(`Writing at (0x[0-9a-fA-F]{8})\.\.\. \((\d+) %\)`)

// ParseProgress scans esptool stdout for "Writing at … (NN %)" lines.
func ParseProgress(out string, cb func(ProgressEvent)) {
	for _, line := range strings.Split(out, "\n") {
		m := progressLine.FindStringSubmatch(line)
		if len(m) != 3 {
			continue
		}
		pct, _ := strconv.Atoi(m[2])
		cb(ProgressEvent{Offset: m[1], Pct: pct})
	}
}

// extractError pulls the most relevant line out of esptool output.
func extractError(out string) string {
	var last string
	for _, line := range strings.Split(out, "\n") {
		line = strings.TrimSpace(line)
		if strings.HasPrefix(line, "A fatal error occurred:") {
			return line
		}
		if line != "" {
			last = line
		}
	}
	return last
}
