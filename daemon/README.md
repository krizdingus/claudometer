# claudometer

A background service that reads your Claude session history from `~/.claude/projects/*.jsonl`, computes session and weekly usage statistics, and serves them on a local HTTP API for a CYD device to poll.

## Installation

### Homebrew (recommended, macOS and Linux)

```bash
brew install krizdingus/tap/claudometer
brew services start claudometer
```

This builds the binary from source (Go is installed as a transparent build-time dependency) and registers `claudometer` as a background service that starts on login. On macOS this uses launchd; on Linux it uses systemd --user. Stop and restart with `brew services stop|restart claudometer`.

### From source (dev / fallback)

```bash
git clone https://github.com/krizdingus/claudometer.git
cd claudometer/daemon
make build
./bin/claudometer
```

Runs in the foreground. To run as a background service without brew, write a launchd plist (macOS) or systemd unit (Linux) yourself — see the `service` block in [the formula][formula].

[formula]: https://github.com/krizdingus/homebrew-tap/blob/main/Formula/claudometer.rb

## Configuration

The daemon reads settings from `~/.config/claudometer/config.json`. On first run, it creates a default config if none exists.

### Config file schema

```json
{
  "plan_tier": "free",
  "session_tokens_override": 0,
  "weekly_all_override": 0,
  "weekly_opus_override": 0,
  "daily_chat_messages_override": 0,
  "listen_addr": "0.0.0.0:7842"
}
```

- `plan_tier` — one of `"free"`, `"pro"`, `"max-5x"`, `"max-20x"`. Default `"free"`. Determines usage caps.
- `session_tokens_override`, `weekly_all_override`, `weekly_opus_override`, `daily_chat_messages_override` — integer overrides for the plan-tier caps. A value of `0` means "no override at this layer; use environment variable or plan default."
- `listen_addr` — TCP address the daemon listens on. Default `"0.0.0.0:7842"`.

### Precedence and environment variables

Settings precedence (highest to lowest):
1. Config file (`~/.config/claudometer/config.json`)
2. Environment variables: `CLAUDOMETER_SESSION_TOKENS`, `CLAUDOMETER_WEEKLY_ALL`, `CLAUDOMETER_WEEKLY_OPUS`, `CLAUDOMETER_DAILY_CHAT_MESSAGES`
3. Plan tier default

Edit the config file and then restart the daemon:

```bash
brew services restart claudometer
```

For dev runs, stop the foreground process and start a new one.

## Pairing a CYD

### Quick pairing (recommended)

    claudometer add-device

This auto-detects a CYD on USB, prompts for WiFi credentials, downloads the latest firmware if the chip is fresh, flashes, pushes provisioning JSON, and waits for the device to come online. Flags for non-interactive use:

    claudometer add-device --port /dev/cu.usbserial-1110 --ssid MyWifi --name desk-cyd

Set `CLAUDOMETER_WIFI_PASSWORD` to keep the password out of your shell history.

Force a re-flash:

    claudometer add-device --reflash

Use a local firmware build (skip the GitHub release download):

    claudometer add-device --firmware ./firmware/.pio/build/esp32dev

### Manual pairing (fallback)

If you'd rather wire it up by hand: `screen /dev/cu.usbserial-XXX 115200`, wait for `READY <mac>`, paste a single JSON line per the schema in `firmware/src/net/usb_provisioner.h`, then edit `~/.config/claudometer/pairings.json` to include the bearer you put in the JSON. The `claudometer add-device` command exists to replace this.

## Logs

### macOS (brew services)

```bash
tail -f $(brew --prefix)/var/log/claudometer.log
```

Error logs go to `claudometer.err.log` in the same directory. Run `brew services info claudometer` for exact paths on your system.

### Linux (brew services)

```bash
journalctl --user -u homebrew.claudometer -f
```

### Foreground dev runs

Logs print to stderr in your terminal.

## Build and test (contributors)

```bash
make build   # builds ./bin/claudometer
make test    # runs Go tests
```

The daemon has no third-party Go dependencies beyond what's in `go.mod`.
