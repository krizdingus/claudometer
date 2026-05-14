# claudometer daemon

The host-side service for the CYD Claude Usage Monitor.

## Build & Run

```bash
make build
./bin/claudometer          # serves on :7842
./bin/claudometer status   # show running state + any pending pairing code
./bin/claudometer reset-pairings
```

## What it does

- Reads `~/.claude/projects/*.jsonl` and `~/.claude.json`
- Computes 5-hour session blocks, weekly budgets, today's per-model usage
- Serves a single `/v1/stats` JSON endpoint to paired CYD devices
- Advertises itself on the LAN via mDNS (`_claudeusage._tcp.local`)
- Pairs new CYDs via 4-digit codes shown by `claudometer status`

## API

| Method | Path | Auth | Purpose |
|---|---|---|---|
| GET | `/v1/stats` | Bearer token | Aggregated usage data for the CYD |
| POST | `/v1/pair-init` | none | CYD requests a pairing code |
| POST | `/v1/pair-verify` | none | CYD submits code + receives token |
| GET | `/v1/status` | none | Health + pending pairing code |

Token storage: `~/.config/claudometer/pairings.json` (mode 0600).

See `docs/superpowers/specs/2026-05-13-cyd-claude-usage-monitor-design.md` for the full design.
