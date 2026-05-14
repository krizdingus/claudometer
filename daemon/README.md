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

The CYD firmware uses a one-shot serial provisioning protocol. See `firmware/src/net/usb_provisioner.h` for protocol details.

### Setup workflow

1. Flash the firmware to your CYD. See [firmware/README.md](../firmware/README.md) for instructions.

2. Connect the CYD via USB and open a serial console:
   - macOS: `screen /dev/cu.usbserial-XXX 115200`
   - Linux: `picocom -b 115200 /dev/ttyUSB0`

3. Wait for the firmware to print `READY <mac>` on the serial console.

4. Send a single JSON line (then Enter):
   ```json
   {"wifi_ssid":"YourWiFi","wifi_password":"...","server_host":"192.168.x.x","server_port":7842,"bearer_token":"YOUR_TOKEN","provision_schema":1}
   ```

5. Create a matching entry in `~/.config/claudometer/pairings.json`. This is an array of pairing objects:
   ```json
   [
     {
       "token": "YOUR_TOKEN",
       "cyd_id": "MAC_ADDRESS",
       "name": "Desk",
       "created_at": "2026-05-14T12:00:00Z"
     }
   ]
   ```

6. The firmware will ACK with `OK`, reboot, and begin polling `/v1/stats` every 30 seconds.

Note: A turnkey `claudometer flash <port>` subcommand is on the roadmap.

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
