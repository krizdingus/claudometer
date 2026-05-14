package flasher_test

import (
	"errors"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/krizdingus/claudometer/daemon/pkg/flasher"
)

func TestDownload_FetchesAndCachesArtifacts(t *testing.T) {
	mux := http.NewServeMux()
	mux.HandleFunc("/releases/download/v0.1.0/bootloader.bin", func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte("boot-bin-bytes"))
	})
	mux.HandleFunc("/releases/download/v0.1.0/partitions.bin", func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte("part-bin-bytes"))
	})
	mux.HandleFunc("/releases/download/v0.1.0/firmware.bin", func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte("firmware-bin-bytes"))
	})
	srv := httptest.NewServer(mux)
	defer srv.Close()

	cacheDir := t.TempDir()
	bundle, err := flasher.DownloadFromBase(srv.URL+"/releases/download", "v0.1.0", cacheDir)
	if err != nil {
		t.Fatalf("Download: %v", err)
	}

	for _, p := range []string{bundle.Bootloader, bundle.Partitions, bundle.Firmware} {
		if !strings.HasPrefix(p, cacheDir) {
			t.Errorf("path %q not under cacheDir %q", p, cacheDir)
		}
		if _, err := os.Stat(p); err != nil {
			t.Errorf("stat %q: %v", p, err)
		}
	}
	data, _ := os.ReadFile(bundle.Firmware)
	if string(data) != "firmware-bin-bytes" {
		t.Errorf("firmware content = %q, want firmware-bin-bytes", data)
	}
}

func TestDownload_SkipsExistingCache(t *testing.T) {
	cacheDir := t.TempDir()
	dir := filepath.Join(cacheDir, "v0.1.0")
	os.MkdirAll(dir, 0o755)
	os.WriteFile(filepath.Join(dir, "bootloader.bin"), []byte("cached-boot"), 0o644)
	os.WriteFile(filepath.Join(dir, "partitions.bin"), []byte("cached-part"), 0o644)
	os.WriteFile(filepath.Join(dir, "firmware.bin"), []byte("cached-firmware"), 0o644)

	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		t.Errorf("unexpected download request: %s", r.URL.Path)
		w.WriteHeader(500)
	}))
	defer srv.Close()

	bundle, err := flasher.DownloadFromBase(srv.URL+"/releases/download", "v0.1.0", cacheDir)
	if err != nil {
		t.Fatalf("Download: %v", err)
	}
	data, _ := os.ReadFile(bundle.Firmware)
	if string(data) != "cached-firmware" {
		t.Errorf("firmware content = %q, want cached-firmware (no re-download)", data)
	}
}

func TestLocalBundle_LoadsThreeFiles(t *testing.T) {
	dir := t.TempDir()
	os.WriteFile(filepath.Join(dir, "bootloader.bin"), []byte("b"), 0o644)
	os.WriteFile(filepath.Join(dir, "partitions.bin"), []byte("p"), 0o644)
	os.WriteFile(filepath.Join(dir, "firmware.bin"), []byte("f"), 0o644)

	bundle, err := flasher.LocalBundle(dir)
	if err != nil {
		t.Fatalf("LocalBundle: %v", err)
	}
	if bundle.Bootloader == "" || bundle.Partitions == "" || bundle.Firmware == "" {
		t.Errorf("bundle = %+v, want all paths set", bundle)
	}
}

func TestLocalBundle_RejectsMissingFile(t *testing.T) {
	dir := t.TempDir()
	os.WriteFile(filepath.Join(dir, "bootloader.bin"), []byte("b"), 0o644)
	os.WriteFile(filepath.Join(dir, "partitions.bin"), []byte("p"), 0o644)

	_, err := flasher.LocalBundle(dir)
	if err == nil {
		t.Fatal("LocalBundle: expected error for missing firmware.bin, got nil")
	}
	if !strings.Contains(err.Error(), "firmware.bin") {
		t.Errorf("error %q should mention firmware.bin", err)
	}
}

func TestParseProgress_ExtractsPercents(t *testing.T) {
	sample, err := os.ReadFile("testdata/esptool-progress-sample.txt")
	if err != nil {
		t.Fatalf("read sample: %v", err)
	}
	var calls []flasher.ProgressEvent
	flasher.ParseProgress(string(sample), func(ev flasher.ProgressEvent) {
		calls = append(calls, ev)
	})

	if len(calls) < 3 {
		t.Fatalf("calls = %d, want >= 3", len(calls))
	}
	gotOffsets := map[string]bool{}
	for _, c := range calls {
		gotOffsets[c.Offset] = true
	}
	for _, want := range []string{"0x00001000", "0x00008000", "0x00010000"} {
		if !gotOffsets[want] {
			t.Errorf("missing progress events for offset %s", want)
		}
	}
}

func TestFlash_RunsEsptoolWithExpectedArgs(t *testing.T) {
	bundle := flasher.FirmwareBundle{
		Bootloader: "/tmp/bootloader.bin",
		Partitions: "/tmp/partitions.bin",
		Firmware:   "/tmp/firmware.bin",
	}

	var gotName string
	var gotArgs []string
	runner := func(name string, args ...string) ([]byte, error) {
		gotName = name
		gotArgs = args
		return []byte("Hash of data verified.\nLeaving...\n"), nil
	}
	err := flasher.FlashWithRunner("/dev/cu.usbserial-1", bundle, runner, nil)
	if err != nil {
		t.Fatalf("FlashWithRunner: %v", err)
	}
	if gotName != "esptool" && gotName != "esptool.py" {
		t.Errorf("runner called with %q, want esptool or esptool.py", gotName)
	}
	joined := strings.Join(gotArgs, " ")
	for _, want := range []string{
		"--port", "/dev/cu.usbserial-1",
		"write_flash",
		"0x1000", "/tmp/bootloader.bin",
		"0x8000", "/tmp/partitions.bin",
		"0x10000", "/tmp/firmware.bin",
	} {
		if !strings.Contains(joined, want) {
			t.Errorf("missing argument %q in: %s", want, joined)
		}
	}
}

func TestFlash_PropagatesRunnerError(t *testing.T) {
	bundle := flasher.FirmwareBundle{Bootloader: "/b", Partitions: "/p", Firmware: "/f"}
	runner := func(name string, args ...string) ([]byte, error) {
		return []byte("Connecting...\nA fatal error occurred: Failed to connect to ESP32\n"),
			errors.New("exit status 1")
	}
	err := flasher.FlashWithRunner("/dev/cu.x", bundle, runner, nil)
	if err == nil {
		t.Fatal("expected error, got nil")
	}
	if !strings.Contains(err.Error(), "Failed to connect") {
		t.Errorf("error should surface esptool stderr; got %v", err)
	}
}
