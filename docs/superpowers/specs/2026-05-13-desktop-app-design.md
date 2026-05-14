# Desktop App (Phase 3 Distribution) — Design

**Date:** 2026-05-13
**Status:** Design
**Supersedes:** Phase 3 portion of `2026-05-13-cyd-claude-usage-monitor-design.md` (flasher site + Homebrew formula + Linux installer)

## Summary

Replace the terminal-only Go daemon and the planned browser-based flasher with a single cross-platform desktop application: download, install, open, plug in CYD, done. The app handles flashing, WiFi/token provisioning over USB, the always-on usage server, and a settings UI. Built with Wails (Go + native webview), shipping for macOS and Windows.

This collapses Phase 3 from three pieces (web flasher + brew formula + Linux installer) into one artifact per OS.

## Goals

- One download, one install, no terminal commands for the end user.
- Cross-platform: macOS and Windows at launch.
- Eliminate the captive-portal WiFi flow and the 4-digit pairing code flow — both happen over USB at flash time.
- Plan tier and budget overrides surfaced as UI settings instead of environment variables.
- Auto-update both the desktop app and the bundled firmware image.
- Keep the existing daemon Go packages (`claudedata`, `stats`, `server`, `pairings`, `routines`, `discovery`) and the frozen `/v1/stats` JSON contract.

## Non-Goals

- Linux distribution. Out of scope for v1. The existing `cydmonitor` Go binary still builds and runs on Linux for users who want to build from source.
- Multi-device support beyond what the daemon already allows.
- Web Serial flashing inside the app. WKWebView on macOS doesn't support Web Serial, so we shell out to a bundled `esptool` binary instead. Behavior is identical on both OSes.
- Mobile companion app.

## Framework choice: Wails

| | Wails | Tauri | Electron |
|---|---|---|---|
| Bundle size | ~15–30 MB | ~15–30 MB | ~120–180 MB |
| Idle RAM | ~50 MB | ~50 MB | ~200 MB |
| Reuses existing Go daemon code | Yes (direct import) | No (Rust rewrite) | No (Node rewrite or Go sidecar) |
| Webview | WKWebView (mac), WebView2 (win) | Same | Bundled Chromium |
| Tray, auto-update, auto-launch | Native support | Native support | Native support |
| Maintenance language(s) | Go + JS/HTML | Rust + JS/HTML | JS/Node + (Go if sidecar) |

Wails wins on this project specifically because the Go daemon already exists, is tested, and works. The other two options trade working Go code for a rewrite.

## Architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│ User's Mac or PC                                                     │
│                                                                      │
│   ~/.claude/projects/*.jsonl   ~/.claude.json                        │
│             │                         │                              │
│             └────────────┬────────────┘                              │
│                          ▼                                           │
│              ┌────────────────────────────────────────┐              │
│              │  CYDMonitor.app  (Wails binary)        │              │
│              │                                        │              │
│              │  ┌──────────────────────────────────┐  │              │
│              │  │ Embedded Go packages (unchanged) │  │              │
│              │  │   claudedata, stats, pairings,   │  │              │
│              │  │   server, routines, discovery    │  │              │
│              │  └──────────────────────────────────┘  │              │
│              │                                        │              │
│              │  ┌──────────────────────────────────┐  │              │
│              │  │ New: flasher (esptool subprocess)│  │              │
│              │  │       provisioner (serial JSON)  │  │              │
│              │  │       settings store (JSON file) │  │              │
│              │  │       tray + auto-launch         │  │              │
│              │  │       webview UI (settings page) │  │              │
│              │  └──────────────────────────────────┘  │              │
│              │                                        │              │
│              │  HTTP API :7842    mDNS advertiser     │              │
│              └────────────┬──────────────┬────────────┘              │
└───────────────────────────┼──────────────┼───────────────────────────┘
                            │ USB (one-time provisioning)              
                            │                                          
                            ▼                                          
                    ┌────────────────┐    LAN ──► CYD (firmware reads  
                    │      CYD       │ ◄──────── NVS, polls /v1/stats) 
                    └────────────────┘                                 
```

## Components

### Reused (Go, unchanged)

- `internal/claudedata` — JSONL parser, file watcher, 30s reload loop
- `internal/stats` — session/weekly/today computations, plan caps, schema v1 types
- `internal/server` — HTTP handlers, bearer auth, `/v1/stats`
- `internal/pairings` — token storage at `~/.config/cydmonitor/pairings.json` (mode 0600)
- `internal/routines` — daily routine counters
- `internal/discovery` — mDNS advertiser

The Wails `main.go` constructs the same services the existing `cmd/cydmonitor/main.go` does, on the same port, with the same lifecycles.

### Removed

- `cmd/cydmonitor/main.go` as the user-facing entrypoint. The file still exists and still builds — useful for developers and Linux users — but it's no longer the shipped artifact.
- The 4-digit pairing code flow on the server side is retained but unused in the desktop-app path. Tokens are minted client-side (in the app) and pushed to the CYD over USB. The server still validates bearer tokens identically.

### New (in Wails app)

**Flasher.** Calls a bundled `esptool` binary as a subprocess. The app ships three artifacts per release: `bootloader.bin`, `partitions.bin`, `firmware.bin`. The flasher writes them to the CYD at the standard ESP32 offsets (0x1000, 0x8000, 0x10000). Reports progress to the UI by parsing esptool stdout.

**USB Provisioner.** After flash, opens the serial port at 115200 baud, waits for the firmware's `READY` line. The firmware's `READY` line includes the ESP32's MAC address: `READY <mac>`. The app uses that MAC as the stable `device_id` for the pairings entry, then sends a single JSON line:

```json
{
  "wifi_ssid": "...",
  "wifi_password": "...",
  "server_host": "192.168.1.42",
  "server_port": 7842,
  "bearer_token": "...",
  "provision_schema": 1
}
```

(`provision_schema` is distinct from the `/v1/stats` `SchemaVersion`; this versions the USB provisioning protocol only.)

Firmware ACKs with `OK`, writes to NVS, reboots. App waits for the CYD to make a request against `/v1/stats` with the freshly minted bearer to confirm end-to-end success.

**Settings store.** JSON file at `~/Library/Application Support/CYDMonitor/settings.json` (mac) / `%APPDATA%\CYDMonitor\settings.json` (win). Schema:

```json
{
  "plan_tier": "max-5x",
  "session_tokens_override": null,
  "weekly_all_override": null,
  "weekly_opus_override": null,
  "daily_chat_messages_override": null,
  "poll_interval_seconds": 30,
  "enabled_screens": ["home", "session", "models", "sonnet", "routines", "budgets"],
  "launch_at_login": true
}
```

Settings replace the `CYDMONITOR_*` env vars one-for-one. The Go services read settings from this file at startup and watch for changes; daemon-internal config types stay the same.

**Tray.** Standard tray icon with menu:
- App name + version (disabled)
- Status indicator (green/yellow/red, see below)
- "Open settings…" (shows window)
- "Add a device…" (starts USB provisioning wizard)
- "Quit"

Closing the window hides it; only "Quit" exits.

Tray status colors:
- Green: server up, ≥1 paired device, JSONL parse succeeded in last cycle
- Yellow: server up, no paired devices, OR JSONL parse stale
- Red: server failed to bind, or JSONL parse erroring

**Auto-launch.** Wails wraps the platform APIs:
- macOS: `SMAppService.loginItem` (modern) or LaunchAgents (legacy)
- Windows: `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`

Default: on. Toggleable in settings.

**Webview UI.** Single-page settings UI. Pages:
- Devices (list of paired CYDs, "Add device" button, per-device "Forget" and "Reflash firmware")
- Settings (plan tier dropdown — `max-5x`, `max-20x`, `pro`, `free` — override numeric inputs, poll interval, screen toggles, launch-at-login)
- About (version, "Check for updates," log file location)

Plain HTML/CSS/JS. No framework requirement; if we end up wanting reactivity, Svelte is the lightest fit. Defer that decision to implementation.

**Auto-update.** Wails has no first-class auto-updater, but `go-update` + a hosted appcast JSON is the standard approach. On Mac we can also use Sparkle via a small Objective-C shim. App checks for updates on launch and once per day. When an update includes a new firmware image, the app shows a "New firmware available — connect your CYD to update" prompt and runs the flasher again, preserving NVS settings (firmware does not erase NVS unless flashed with `--erase-all`, which we don't do for updates).

## Firmware changes

The captive portal and pairing-code flows on the CYD go away. New boot logic:

1. Read NVS keys: `wifi_ssid`, `wifi_password`, `server_host`, `server_port`, `bearer_token`.
2. If all present → connect to WiFi, start polling `/v1/stats` with bearer.
3. If any missing → enter **provisioning mode**:
   - Print `READY\n` on Serial at 115200 baud.
   - Block on serial read for up to 60 seconds.
   - On valid JSON receipt: validate schema, write NVS keys, print `OK\n`, `ESP.restart()`.
   - On invalid input or timeout: print `ERR <reason>\n`, blink LED, retry from step 1 after 5s.

Screens to remove:
- The captive portal AP setup screen and its HTTP form handler
- The 4-digit pairing code display screen
- The "waiting for daemon" mDNS discovery screen (still useful, but the flow is now: NVS has `server_host` directly, no discovery needed for the happy path)

mDNS *discovery* on the firmware is kept as a fallback: if the firmware can't reach `server_host` (e.g. router gave the Mac a new IP), it falls back to mDNS and updates NVS with the new IP. This is the only reason mDNS stays in the design.

The six display screens, fonts, layout, and `/v1/stats` parsing are unchanged. `SchemaVersion = 1` stays.

## Data flow: first-run provisioning

```
User clicks "Add device" in app
     │
     ▼
App: enumerate serial ports, ask user to pick (or auto-pick if one CH340/CP2102 is present)
     │
     ▼
App: prompt for WiFi SSID + password (suggests SSIDs visible to host machine)
     │
     ▼
App: prompt for plan tier (or read from settings if already set)
     │
     ▼
App: mint a fresh bearer token (32 bytes of crypto/rand, base64)
     │
     ▼
App: store {device_id, token} in pairings.json
     │
     ▼
App: shell out to esptool: erase_flash → write_flash 0x1000 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
     │
     ▼
App: open serial @ 115200, wait for "READY"
     │
     ▼
App: send provisioning JSON, wait for "OK"
     │
     ▼
App: close serial, wait for the CYD to make its first /v1/stats request (≤ 30s)
     │
     ▼
App: show "Device connected" success
```

## Error handling

- **No serial port detected:** UI shows "Plug in your CYD via USB" with troubleshooting link (drivers, cable-vs-charging-only USB).
- **Multiple serial ports:** UI shows a dropdown.
- **esptool failure (sync, bad hash, etc.):** Surface esptool's stderr, log full output, offer "Retry" and "Open log."
- **Serial provisioning timeout:** Retry once automatically, then prompt user to power-cycle CYD.
- **WiFi creds wrong:** Firmware can't tell us — provisioning ACK happens before WiFi attempt. If no `/v1/stats` request arrives within 60s of "OK," app shows "Device didn't connect — check WiFi password" with re-provision button.
- **Port :7842 already in use:** App detects on launch, tries :7843, :7844 up to :7849. Updates settings + re-provisions any paired devices (firmware reconnects via mDNS fallback).
- **JSONL parse error:** Tray goes yellow, status panel shows error. Daemon retries every 30s.
- **`~/.claude/projects/` doesn't exist:** App on first launch shows "Claude Code not detected. Install Claude Code first, then open this app." Doesn't crash; daemon returns empty stats.

## Distribution

### macOS

- Build: `wails build -platform darwin/universal` produces `CYDMonitor.app` (arm64 + x86_64).
- Sign with Apple Developer cert (user has one).
- Notarize via `xcrun notarytool`.
- Package as `.dmg` with drag-to-Applications UI.
- Auto-update: Sparkle-style appcast hosted on GitHub releases.

### Windows

- Build: `wails build -platform windows/amd64`.
- Package as NSIS installer (`.exe`) using Wails' built-in NSIS support.
- Signing: ship unsigned at v1, apply to SignPath.io once we have a public repo + a few releases. Fallback: Azure Trusted Signing (~$10/mo).
- Auto-update: same appcast scheme; NSIS installer auto-elevates if needed.

### Release artifacts per version

- `CYDMonitor-vX.Y.Z.dmg` (mac)
- `CYDMonitor-vX.Y.Z.exe` (win installer)
- `firmware-vX.Y.Z.bin` (sidecar, embedded in app bundles)
- `appcast.xml` (update feed)

## Testing strategy

- **Unit:** all existing daemon Go tests pass unchanged. New code (flasher subprocess wrapper, USB provisioner, settings store) gets focused unit tests with mocked io.
- **Integration:** end-to-end provisioning test on a real CYD, run manually before each release. Automate the `esptool → serial → first stats request` flow on a CI box with a CYD attached if it ever becomes worth it.
- **Cross-platform:** test the Windows build on a Windows VM or physical machine before release. Mac is the primary dev platform.

## Migration

Existing users (the developer, basically — Phase 1+2 is fresh) re-flash their CYD via the new app's "Add device" flow. The daemon Go binary continues to work for anyone running it from terminal; this is opt-in distribution, not a forced upgrade.

## What stays frozen

- `/v1/stats` JSON contract and `SchemaVersion = 1`
- All six daemon Go packages
- All six firmware display screens, fonts, layout
- Tray HTTP port default :7842
- Pairings file location `~/.config/cydmonitor/pairings.json`

## Open questions for implementation

- Wails frontend framework: plain JS vs Svelte. Pick at implementation start. Lean toward plain JS unless the settings UI grows complex.
- Appcast hosting: GitHub releases JSON file vs separate static site. Start with GitHub.
- Embedding firmware images: `go:embed` is the obvious choice. Confirm Wails packaging picks them up.
- How to expose pre-update settings/pairings on Windows when the installer replaces the binary. NSIS upgrade mode preserves `%APPDATA%` files by default; verify behavior.
