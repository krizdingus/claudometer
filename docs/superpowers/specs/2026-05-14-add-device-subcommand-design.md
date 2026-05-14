# `claudometer add-device` Subcommand — Design

**Date:** 2026-05-14
**Status:** Design

## Summary

A subcommand on the existing `claudometer` Go binary that handles end-to-end CYD onboarding: detect the device on USB, flash firmware if needed, mint a bearer-token pairing, push WiFi credentials + token over serial, and verify the new device comes online. Replaces the manual workflow (open `screen`, wait for `READY`, hand-craft JSON, paste, edit pairings.json) with one command.

## Goals

- One command takes a fresh ESP32-2432S028 (R or C) from "plugged in" to "polling the daemon" without the user touching `screen`, `pyserial`, or `pairings.json`.
- Same command works for "re-provisioning" an already-flashed CYD (e.g. user changed WiFi password) without re-flashing.
- No new always-on infrastructure. Firmware artifacts come from GitHub releases on demand and are cached locally.
- Cross-platform (macOS and Linux). Windows is out of scope for this subcommand, consistent with the rest of the daemon's current target set.

## Non-Goals

- A `remove-device` subcommand. For v1, users edit `pairings.json` directly to remove devices.
- OTA firmware updates over WiFi. Re-flashing always requires USB.
- A browser-based setup UI. The CLI is the only entrypoint.
- Auto-detection of WiFi credentials from the host's currently-joined network (a nice ergonomic but not necessary for v1).
- GitHub release automation. The first releases are cut manually; GoReleaser is a later plan.

## User Experience

```
$ claudometer add-device
Looking for a CYD on USB...
Found: /dev/cu.usbserial-1110 (Silicon Labs CP210x)

WiFi SSID: MyWifi-5G
WiFi password: ****************

Checking if firmware is already flashed... yes (responded with READY)
Skipping flash.

Pairing as cyd-1f8a3c.
Pushing WiFi credentials and bearer token over serial... done.
Waiting for cyd-1f8a3c to come online... done (first stats poll received).

cyd-1f8a3c is connected.
```

A fresh chip flow adds a flashing step:

```
$ claudometer add-device
Looking for a CYD on USB...
Found: /dev/cu.usbserial-1110 (Silicon Labs CP210x)

WiFi SSID: MyWifi-5G
WiFi password: ****************

Checking if firmware is already flashed... no.
Downloading firmware v0.1.0 from GitHub releases... done (1.2 MB).
Flashing /dev/cu.usbserial-1110...
  Writing bootloader.bin    [====================] 100%
  Writing partitions.bin    [====================] 100%
  Writing firmware.bin      [====================] 100%
  Verifying... done.

Pairing as cyd-1f8a3c.
Pushing WiFi credentials and bearer token over serial... done.
Waiting for cyd-1f8a3c to come online... done.

cyd-1f8a3c is connected.
```

## Architecture

Three new packages under `daemon/pkg/`, plus a thin CLI orchestrator and a subcommand dispatch line in main.

```
                           +-----------------+
                           |     cli/        |
                           |  add_device.go  |
                           +--------+--------+
                                    |
            +-----------------------+----------------------+
            |                       |                      |
            v                       v                      v
    +---------------+      +----------------+     +----------------+
    |   flasher/    |      | provisioner/   |     |  HTTP POST     |
    |  (new pkg —   |      |  (new package) |     |  /v1/admin/pair|
    |   esptool     |      |  serial port + |     |  on the running|
    |   wrapper +   |      |  JSON push)    |     |  daemon → token|
    |   downloader) |      |                |     |                |
    +---------------+      +----------------+     +----------------+
```

### `daemon/pkg/flasher/`

- `Download(version string, cacheDir string) (FirmwareBundle, error)` — fetches `bootloader.bin`, `partitions.bin`, `firmware.bin` from `https://github.com/krizdingus/claudometer/releases/download/<version>/<file>`. Falls back to "latest" tag if version is empty. Caches at `<cacheDir>/<version>/` and short-circuits on subsequent calls.
- `Flash(port string, bundle FirmwareBundle, progress func(stage string, pct int)) error` — shells out to `esptool` with the standard ESP32 offsets (0x1000, 0x8000, 0x10000). Parses esptool's stdout for percent-complete and routes to the progress callback.
- `LocalBundle(dir string) (FirmwareBundle, error)` — alternative source: read the three `.bin` files from a local directory (for `--firmware` flag and dev workflows).
- esptool itself is a brew-installed Python tool, added to the formula's `depends_on` list.

### `daemon/pkg/provisioner/`

- `EnumeratePorts() ([]Port, error)` — wraps `go.bug.st/serial.GetPortsList()` and filters to known CYD USB-serial chip vendor/product IDs (CP210x, CH340, FTDI). Returns name + descriptor for each.
- `Probe(port string, timeout time.Duration) (mac string, found bool, err error)` — opens serial at 115200 baud, watches for `READY <MAC>` for up to `timeout`. Returns `(mac, true, nil)` if found, `(_ , false, nil)` on timeout, error only on serial errors. Used to decide whether to flash.
- `Provision(port string, creds Creds, timeout time.Duration) error` — opens serial, optionally waits for `READY` if not already seen, sends one JSON line per the existing `provision_schema: 1` contract, waits for `OK\n` ACK. Returns an error on `ERR <reason>` or timeout.
- The Creds struct mirrors the existing firmware-side `ProvisioningCreds` (in `firmware/src/net/usb_provisioner.h`).

### `daemon/pkg/cli/add_device.go`

Orchestrates the flow described under "User Experience." Owns prompting, flag parsing, and the verification step (polls the daemon's `/v1/stats` access log or — simpler — inspects the pairings file for an updated `last_seen` timestamp after the device starts polling). For v1, the verification is a 60-second wait on a `WaitForDevicePoll(mac)` helper that the existing server package surfaces.

### Pairing minting

The `pairings.Store` keeps its state in memory (per `daemon/pkg/pairings/store.go`) — `Add` writes through to disk, but a second process writing to the same file would not be visible to the running daemon until it restarts. So `add-device` cannot just write directly to `pairings.json`; it must hand the request to the running daemon's process.

The daemon gains a new HTTP endpoint, `POST /v1/admin/pair`, that:

- Only accepts requests whose `RemoteAddr` resolves to a loopback address (`127.0.0.1` or `::1`). Returns 403 to any other source.
- Accepts a JSON body `{"cyd_id": "<mac>", "name": "<label>"}`.
- Calls the existing `pairings.Store.Add(cydID, name)`, which mints a 32-byte hex token, persists it, and updates the in-memory map.
- Returns `{"token": "<hex>"}`.

`add-device` calls this endpoint to mint the token, then includes it in the provisioning JSON pushed over serial. The daemon process knows about the new pairing immediately, so the verification step (waiting for the CYD's first authenticated `/v1/stats` poll) works without a daemon restart.

If the daemon is not running, `add-device` aborts before reaching the mint step with a clear "start the daemon first" message.

The pre-existing `/v1/pair-init` and `/v1/pair-verify` endpoints are unrelated and stay as-is for any future flows that need them; the new admin endpoint is a simpler shape for the local-flash use case.

## Data flow

```
1. User runs `claudometer add-device`
   |
   v
2. CLI enumerates serial ports, picks or prompts
   |
   v
3. CLI prompts for WiFi SSID/password (unless flags provided)
   |
   v
4. provisioner.Probe(port, 5s)
   |
   +-- found READY <MAC>: skip to step 7
   |
   +-- no READY: continue to step 5
   |
   v
5. flasher.Download(version, ~/.cache/claudometer/firmware)
   (or flasher.LocalBundle if --firmware passed)
   |
   v
6. flasher.Flash(port, bundle, progressCallback)
   |
   v
7. POST /v1/admin/pair { cyd_id: MAC, name: derived or --name } → token
   |
   v
8. provisioner.Provision(port, Creds{wifi, server_host, server_port, bearer})
   |
   v
9. Wait up to 60s for the new device's first /v1/stats poll
   |
   v
10. Print success or failure summary
```

## Error handling

| Scenario | Behavior |
|---|---|
| No USB serial ports detected | Print troubleshooting checklist (driver, cable, plug-in order). Exit 1. |
| Multiple ports detected | Print numbered list, prompt to pick. Exit 1 if user aborts. |
| Serial open fails (busy, permission denied) | Print port and error. Suggest `lsof` on macOS or group membership on Linux. Exit 1. |
| `Probe` times out and `--no-flash` was passed | Print "no firmware detected; remove --no-flash or run `claudometer add-device` again without it." Exit 1. |
| GitHub release fetch fails (no network, 404) | Print URL attempted and error. Suggest `--firmware <local-path>` as workaround. Exit 1. |
| esptool not installed | Print "esptool not found in PATH. Install with `brew install esptool` or `pipx install esptool`." Exit 1. |
| esptool fails (sync error, bad hash) | Surface esptool's stderr. Suggest retry (`hold the BOOT button and try again`). Exit 1. |
| `Provision` returns `ERR` from firmware | Print firmware's error reason. Exit 1. |
| Provisioning ACK received but no /v1/stats poll within 60s | Print "device received credentials but didn't connect — check WiFi password and signal strength. Run `claudometer add-device --reflash` to retry." Exit code 2 (distinguishes "partial success" from "failure"). |
| Daemon not running | Detect by attempting `GET http://127.0.0.1:7842/v1/status` before starting. Print "claudometer daemon is not running. Start it with `brew services start claudometer`." Exit 1. |

## Testing strategy

- **`flasher/`:** unit tests for `Download` (mock HTTP server serving fake .bin files), `LocalBundle` (fixture dir), and bundle path resolution. `Flash` itself is tested against esptool stdout fixtures — the parser logic is the only interesting part; the actual esptool subprocess invocation gets a basic integration test that runs only when `CLAUDOMETER_E2E_FLASH_PORT` env var is set, against a real attached device.
- **`provisioner/`:** unit tests for `EnumeratePorts` (filter logic with fake port lists), `Probe` and `Provision` against a fake serial port implementation. The existing firmware-side tests already cover the JSON parsing; provisioner just needs to cover the host side.
- **`cli/add_device.go`:** integration-style tests that wire the orchestrator against mocked flasher + provisioner, exercising the flow control (skip flash on probe success, continue on probe timeout, error paths).
- **End-to-end:** a `make e2e-flash` Makefile target that runs the full subcommand against a real CYD on `$CLAUDOMETER_E2E_FLASH_PORT`, manually invoked before each release.

No new third-party deps beyond `go.bug.st/serial` (which is the de facto Go serial library, used by ESP-IDF tooling, OpenBSD package managers, etc.).

## Distribution

- Brew formula gains `depends_on "esptool"`.
- A GitHub release (manually cut for v0.1.0) attaches three artifacts: `bootloader.bin`, `partitions.bin`, `firmware.bin`, built via `cd firmware && pio run -e esp32dev` and copied from `.pio/build/esp32dev/`.
- `claudometer add-device` defaults to fetching from `releases/latest`; users can pin a version with `--firmware-version v0.1.0`.

## Future work (out of scope for this spec)

- `claudometer remove-device <mac>` — explicit pairing removal.
- `claudometer relpair` — re-provision an already-flashed CYD without going through the flash detection logic.
- OTA firmware updates over WiFi (push firmware from the daemon to a paired CYD).
- GoReleaser-driven firmware release automation, signed binaries.
- A browser-based setup wizard at `http://localhost:7842/setup`.
- Windows support — needs separate port enumeration logic and an esptool delivery story (probably bundled rather than `brew install`).
