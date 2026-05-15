# Claudometer

A glanceable usage monitor for [Claude Code](https://claude.com/claude-code) that lives on a $15 ESP32 dev board on your desk. Live session burn, weekly caps, per-model breakdown, and the routines you've configured — without taking screen real estate or yet another menu bar item.

## What it does

Seven screens, swipeable by touch:

- **Home** — session burn % as the hero, weekly cap underneath, today's tokens and spend at the bottom. The glance view.
- **Session** — big arc with the current 5-hour block percentage and time until reset.
- **Budgets** — all-models weekly cap as the hero, Opus-only cap as a secondary row, plan and reset countdown below.
- **Models** — today's spend in dollars as the hero, then per-model token bars (Opus / Sonnet / Haiku).
- **Routines** — next scheduled run as the hero, then the configured routines with status pills and recent activity.
- **Settings** — light/dark theme toggle, brightness stepper, and carousel auto-rotation toggle. All persist across reboots.
- **Device** — hostname, IP, WiFi SSID and RSSI, firmware version, and daemon health.

A status bar across the top shows your plan, the current time (from the host daemon), and a daemon-health indicator. With carousel on, the data screens auto-advance every 10 s (paused for 30 s after any touch; Settings and Device never auto-display). Long-press anywhere to factory-reset NVS and re-pair from scratch.

The host daemon reads Claude Code's local JSONL logs in `~/.claude/projects/` directly. No cloud round-trip, no API key. The CYD polls the daemon over your LAN every few seconds.

## What it doesn't

- **Doesn't track Claude.ai chat usage.** Cookie-paste auth is fragile and there's no official API. Maybe later.
- **Doesn't push commands to Claude.** Read-only display. If you want to control your session, use Claude Code itself.
- **No OTA updates yet.** Firmware updates need a USB cable to the CYD. The daemon updates itself via `brew upgrade`.
- **macOS and Linux only on the host side.** No Windows installer yet.

## Hardware

You need a "Cheap Yellow Display" — the ESP32-2432S028R, sold under that nickname on AliExpress, eBay, and Amazon for $10–15. 240×320 ST7789 panel, XPT2046 resistive touch, micro-USB.

Other ESP32 boards with a 240×320 TFT will probably work with pin tweaks in `firmware/src/pins.h`, but I haven't tested any.

## Install

The daemon installs via Homebrew. macOS 12+ or any modern Linux with Homebrew. Go is pulled as a transparent build dep; you don't need it installed yourself.

```bash
brew tap krizdingus/tap
brew install claudometer
brew services start claudometer
```

That registers a launchd (macOS) or systemd-user (Linux) service that runs `claudometer` in the background and survives reboots.

## Setup

Plug the CYD in via USB, then run:

```bash
claudometer add-device --port /dev/cu.usbserial-XXXX
```

The subcommand auto-detects ports if you only have one CYD plugged in (omit `--port`). It will:

1. Flash firmware if the device doesn't have it (downloads the latest `.bin` from GitHub releases)
2. Prompt for your WiFi SSID and password
3. Prompt for your Claude plan tier (`free`, `pro`, `max-5x`, `max-20x`)
4. Mint a bearer token, push everything to the device's NVS via USB serial, restart the daemon, and wait for the CYD to come online

When the CYD shows the home screen, you're done. Unplug it, move it wherever you want it on WiFi.

## Changing plan tier later

```bash
claudometer set-plan max-5x
```

Writes to `~/.config/claudometer/config.json` and restarts the service. The cap math updates immediately.

## Updating firmware

```bash
claudometer add-device --reflash --port /dev/cu.usbserial-XXXX
```

Flashes the latest firmware while preserving the existing pairing — you don't have to re-enter WiFi creds.

## Configuration

Edit `~/.config/claudometer/config.json` directly for cap overrides:

```json
{
  "plan_tier": "max-5x",
  "session_tokens_override": 0,
  "weekly_all_override": 0,
  "weekly_opus_override": 0,
  "daily_chat_messages_override": 0,
  "listen_addr": "0.0.0.0:7842"
}
```

Any non-zero override wins over the plan-tier default. `CLAUDOMETER_*` environment variables also work as a middle layer (file > env > plan default).

After editing the file by hand, restart the service:

```bash
brew services restart claudometer
```

## Build from source

The brew formula already builds from source. If you'd rather poke at the code:

```bash
git clone https://github.com/krizdingus/claudometer.git
cd claudometer/daemon
go build ./cmd/claudometer
./claudometer
```

For firmware:

```bash
cd claudometer/firmware
pio run -e esp32dev
```

You'll need [PlatformIO](https://platformio.org/) installed.

## Roadmap

Things I might add if I feel like it. No timeline, no promises.

- **OTA firmware updates** — push new firmware over the LAN from the daemon to paired CYDs. Right now you need a USB cable to update.
- **Browser-based setup UI** at `http://localhost:7842/setup` — edit config, manage paired devices, kick off pairings without the CLI.
- **Pre-built signed binaries via GoReleaser** — cuts brew install time and removes the Go build dep.
- **Windows installer** — Scoop bucket or MSI. No estimate.
- **Real Claude.ai integration** — only if Anthropic ships an official usage API.

## License

[MIT](./LICENSE).
