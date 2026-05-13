#!/usr/bin/env bash
set -euo pipefail

# 1. Build firmware
cd "$(dirname "$0")/.."
pio run -e esp32dev

# 2. Start daemon if not running
if ! curl -fs http://127.0.0.1:7842/v1/status >/dev/null; then
  echo "starting daemon..."
  (cd ../daemon && make build && ./bin/cydmonitor &)
  sleep 2
fi

# 3. Confirm mDNS advertises
echo
echo "looking for daemon on the LAN..."
dns-sd -B _claudeusage._tcp local. &
DNS_PID=$!
sleep 3
kill $DNS_PID 2>/dev/null || true

# 4. Upload firmware
echo
read -p "plug in CYD, then press enter to flash..."
pio run -e esp32dev -t upload

# 5. Walk through pairing on-device
cat <<'EOF'

Manual verification checklist (mark ✓ as you go):
  [ ] PROVISION screen visible, AP SSID matches "ClaudeMonitor-XXXX"
  [ ] After joining AP + picking home WiFi, device reboots and shows DISCOVER
  [ ] DISCOVER advances to PAIR within ~5s, showing host + 4-digit code
  [ ] Daemon CLI reports the same code: ./daemon/bin/cydmonitor status
  [ ] Tapping Confirm on the CYD advances to the Session screen
  [ ] All six screens are swipeable
  [ ] Killing the daemon (kill %1) flips chrome dot to yellow within ~30s
  [ ] Restarting the daemon flips chrome dot back to green
  [ ] Holding the screen 5s returns to PROVISION

EOF
