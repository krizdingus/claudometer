# CYD Claude Usage Monitor — Design

**Date:** 2026-05-13
**Status:** Design

## Summary

A small always-on display that shows your Claude usage at a glance. The hardware is an ESP32 "Cheap Yellow Display" (CYD) — a $10 dev board with a 240×320 TFT — and the software is a two-piece system: a host daemon on the user's Mac/Linux box that reads Claude data, and ESP32 firmware that fetches aggregated stats over WiFi and renders them.

The goal of the project is a "plug it in, flash it, it works" experience for an existing Claude Code / Claude.ai user.

## Goals

- Display six rotating screens of Claude usage on a 240×320 display: current session, all-models breakdown, Sonnet detail, Claude.ai chat, daily Routines, and budgets.
- Setup time under 5 minutes from unboxing for a user with a Mac and an existing Claude subscription.
- Browser-based firmware flashing — no Arduino IDE, no esptool, no `platformio` commands for the end user.
- The CYD has zero credentials on it. All Claude auth lives on the host machine in the daemon.
- The daemon's local web UI works as a standalone dashboard. The CYD is the always-on glance surface, the web UI is the deep dive.

## Non-Goals

- Cloud-hosted backend. The daemon is local-only. We don't run servers for users.
- Mobile app. The setup flow uses a captive portal and a browser; that's enough.
- Multiple CYDs paired to one daemon at launch. Architecture supports it; we ship single-pairing first.
- Anthropic API consumption monitoring (the "API console" kind of usage). This product targets Claude Code + Claude.ai subscribers.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│ User's Mac                                                       │
│                                                                  │
│   ~/.claude/projects/*.jsonl   ~/.claude.json                    │
│             │                         │                          │
│             └────────────┬────────────┘                          │
│                          ▼                                       │
│              ┌────────────────────────┐                          │
│              │  cydmonitor daemon     │                          │
│              │  (Go, launchd/systemd) │                          │
│              │                        │                          │
│              │  • JSONL parser        │                          │
│              │  • Session computer    │                          │
│              │  • Budget tracker      │                          │
│              │  • mDNS advertiser     │                          │
│              │  • HTTP API :7842      │                          │
│              │  • Web UI              │                          │
│              └───────────┬────────────┘                          │
└──────────────────────────┼───────────────────────────────────────┘
                           │ LAN
                           │ mDNS: _claudeusage._tcp.local
                           │ HTTP: /v1/stats (every 5–30s)
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│ CYD (ESP32-2432S028R/C)                                          │
│                                                                  │
│   WiFiManager → mDNS scan → HTTP client → LVGL renderer          │
│                                                                  │
│   NVS: WiFi creds + daemon pairing token                         │
└─────────────────────────────────────────────────────────────────┘
```

Three components, each independently testable:

1. **`cydmonitor` daemon** (host) — reads Claude data, serves JSON, runs web UI.
2. **CYD firmware** (ESP32) — fetches JSON, renders six screens.
3. **`cydmonitor.dev/flash`** (static site) — ESP Web Tools flasher.

## Hardware Target

**Primary:** ESP32-2432S028C — the capacitive-touch variant of the classic 2.8" Cheap Yellow Display. ESP32-WROOM-32, 4MB flash, ILI9341 240×320 TFT, FT6336 capacitive touch, microSD slot. Roughly $10–15 on AliExpress / Amazon.

**Also supported:** ESP32-2432S028R (resistive touch, XPT2046). We recommend the capacitive variant for swipe UX but the resistive variant must work.

A single firmware binary targets both variants. On boot, the firmware probes for an FT6336 on I²C; if present, capacitive driver loads. Otherwise it falls back to XPT2046 over SPI and runs a one-time touch-calibration screen.

## The Daemon

### Language & distribution

Go. A single static binary distributed via Homebrew (`brew install cydmonitor`) and a one-line curl installer for Linux. The Go choice is for distribution simplicity: one binary, no runtime dependency.

The binary registers itself as a `launchd` service on macOS (`~/Library/LaunchAgents/dev.cydmonitor.plist`) and a `systemd --user` service on Linux. It runs at user login, not as root.

### Data sources

| Source | Status | Notes |
|---|---|---|
| `~/.claude/projects/**/*.jsonl` | Phase 1 | Per-message records: timestamps, model, token counts. Used for sessions, all-models, per-model. |
| `~/.claude.json` | Phase 1 | Account/settings metadata. Plan tier. |
| Anthropic billing API (if API key present) | Phase 2 | For users on API plans. Falls back gracefully if absent. |
| Routines metadata | Phase 1 | `claude routines list --json` shell-out. The daemon caches results for 60s. |
| Claude.ai message counts | Phase 3 | Requires session cookie. Pasted via web UI, stored in macOS Keychain / Linux Secret Service. May ship behind a feature flag. |

We model phase 1 as the MVP. Phase 2 and 3 ship later without changing the firmware contract.

### Computation: session blocks, budgets

The daemon's parser logic is modeled on the open-source `ccusage` tool's algorithm:

- A **session block** is a rolling 5-hour window. A new block starts at the first message after a ≥5-hour idle gap, and contains every subsequent message until the next 5-hour gap. The block's "used" percentage is computed against the plan-tier token cap.
- **Weekly budgets** track a Monday-09:00-local-time reset window per Anthropic's Max plan rules. Per-model caps (Opus, Sonnet) tracked separately from the overall cap.
- All cost estimates use Anthropic's published pricing table, embedded in the daemon and versioned.

### HTTP API

The daemon binds to `0.0.0.0:7842`. The web UI on `localhost` is reachable without a bearer token; LAN clients must present a valid pairing token to call `/v1/stats`. Single endpoint contract:

```
GET /v1/stats?screens=session,models,sonnet,chat,routines,budgets
Authorization: Bearer <pairing-token>

200 OK
{
  "schema": 1,
  "generated_at": "2026-05-13T14:23:00-07:00",
  "session": { "pct_used": 67, "minutes_remaining": 98, "resets_at": "16:01", "models": [...] },
  "models_today": { "total_tokens": 412000, "by_model": [...] },
  "sonnet": { "weekly_pct": 58, "used": 1160000, "cap": 2000000, "pace": "on_track" },
  "chat": { "messages_today": 42, "daily_cap": 200, "resets_at": "00:00" },
  "routines": [ { "name": "babysit-prs", "status": "ok", "last_run": "12:00", "next_run": "17:00" }, ... ],
  "budgets": { "code_all": 71, "code_opus": 89, "chat": 22, "plan": "max-20x", "resets_in": "3d19h" }
}
```

The CYD requests only the screens it's currently rendering (and the next/previous one for swipe responsiveness). Response is gzipped. Typical payload ~2 KB.

### mDNS

The daemon advertises `_claudeusage._tcp.local` on the LAN with TXT records:

```
host=krizzo's MacBook
version=0.3.1
schema=1
```

### Pairing

First-time pairing: CYD discovers the daemon via mDNS, the daemon shows a 4-digit code in its tray-icon menu (and in `cydmonitor status` on the CLI). User confirms the code on the CYD's touch screen. Daemon stores `{cyd_id, token}` pairs; CYD stores the token in NVS. Token is a 256-bit random; no rotation in v1.

### Web UI

`http://localhost:7842/` — a richer dashboard than the CYD shows. Same data, with charts and history. Built as a small SPA (preact + vite, embedded in the binary via `embed.FS`). The web UI is also where the user configures phase 3 things (Claude.ai cookie, polling interval, theme).

## The Firmware

### Stack

- **Framework:** PlatformIO + Arduino-ESP32.
- **Graphics:** LVGL 9.x driving the ILI9341 via LovyanGFX.
- **Touch:** LovyanGFX abstraction; runtime detection of FT6336 (capacitive) vs XPT2046 (resistive).
- **Network:** Built-in WiFi + HTTPClient. JSON parsing via ArduinoJson with a fixed 8 KB doc.
- **WiFi onboarding:** WiFiManager library, custom captive portal page (Anthropic-branded).
- **mDNS:** ESP32 built-in mDNS to find the daemon.

LVGL is overkill for six static-ish screens but gives us anti-aliased rings, gradients, and rounded bars cheaply. ESP32-WROOM has 520 KB SRAM, enough for LVGL's draw buffer at 240×320.

### Screen structure

Six screens in a circular swipe carousel (LVGL `lv_tileview`):

1. **Session** — 5-hour block ring with % used, minutes remaining, top-two models.
2. **All Models · Today** — three horizontal bars (Opus / Sonnet / Haiku), total + estimated cost.
3. **Sonnet** — weekly budget bar, today vs 7-day avg, pace indicator.
4. **Claude.ai · Chat** — daily messages used/cap, artifacts created, projects active.
5. **Routines** — vertical list of up to 5 routines with status pill (ok / slow / fail / queued).
6. **Budgets** — three weekly caps (code-all, code-opus, chat), plan tier, reset countdown, warning pill if any cap >85%.

A persistent **status bar** (top, 18 px) shows: daemon health dot, time. A **page-pip footer** (bottom, 18 px) shows current screen.

The visual language is dark (`#0E0E10` background), Claude-orange accent (`#D97757`), with cool blue (`#6F9EFF`) for non-warning bars and red (`#E85C5C`) for danger thresholds. Yellow (`#F5D24A`) is reserved for the device bezel itself and for highlight chrome.

### State machine

```
            ┌──────────────┐
   boot ──► │  PROVISION   │ ── WiFi creds saved ──┐
            └──────────────┘                       │
                                                   ▼
                                          ┌────────────────┐
                                          │  DISCOVER mDNS │
                                          └────────────────┘
                                                   │
                                       no daemon ◄─┴─► found
                                                       │
                                              ┌────────▼────────┐
                                              │  PAIR (code)    │ ── confirmed ─┐
                                              └─────────────────┘               │
                                                                                ▼
                                                                     ┌──────────────────┐
                                                                     │  POLL & RENDER   │
                                                                     └──────────────────┘
                                                                              │
                                       long-press 5s ──── reset NVS ──────────┘
```

### Polling

Two cadences:
- **Active screen** (session, all-models): 5 s.
- **Idle screens** (sonnet, chat, routines, budgets): 30 s.

The CYD requests only the screens within ±1 of the current screen, so most polls are ~2 KB.

On daemon unreachable: show a desaturated last-known state with a "stale" indicator in the status bar, retry with exponential backoff to 60 s, then mDNS-rediscover.

### Persistence

ESP32 NVS partition stores:
- WiFi SSID + PSK
- Daemon pairing token (32 bytes)
- Daemon last-known hostname (for fast reconnect, validated against mDNS on boot)
- Touch calibration values (resistive variant only)

Long-press the screen for 5s on any screen → factory reset confirmation → clears NVS → returns to PROVISION state.

## Setup Flow

1. **Flash via browser.** User plugs the CYD into their Mac, opens `cydmonitor.dev/flash` in Chrome/Edge, clicks Install. ESP Web Tools writes the firmware over Web Serial in ~30 s.
2. **CYD boots into AP mode.** SSID `ClaudeMonitor-XXXX`, captive portal at `192.168.4.1`. User picks home WiFi, enters password. CYD reboots into WiFi mode.
3. **Install the daemon.** `brew install cydmonitor` on macOS, `curl -sf get.cydmonitor.dev | sh` on Linux. Daemon installs as a user service and auto-starts.
4. **Auto-discover & pair.** CYD finds the daemon via mDNS, shows the daemon's hostname and a 4-digit code. User reads the code from the daemon's tray icon (macOS) or `cydmonitor status` (Linux), taps to confirm on the CYD.
5. **(Optional) Sign in to Claude.ai.** User visits `localhost:7842` in a browser to add a Claude.ai session cookie if they want chat/Routines stats beyond what's in local files.

If a user already had the daemon installed when they flash the CYD, step 4 happens automatically — they only have to confirm the pairing code.

## Build sequencing

Three components ship in this order:

1. **Daemon MVP** — JSONL parser, session/budget computation, HTTP API, mDNS, pairing. No web UI yet. Tested end-to-end with `curl`.
2. **Firmware MVP** — WiFi onboarding, mDNS discovery, pairing, all six screens (Routines + chat screens render with empty-state until phase 3). Tested against the live daemon.
3. **Flasher site + brew formula + installer script** — the distribution layer. Until this exists, contributors flash via PlatformIO and install the daemon via `go install`.
4. **Phase 2/3** — Anthropic billing API, Claude.ai cookie auth, OTA firmware updates from the daemon, web UI dashboard.

Each component has its own subdirectory in the repo: `daemon/`, `firmware/`, `flasher/`, `web/`.

## Error Handling

The product runs on a desk for weeks at a time. Failure modes that must degrade gracefully:

- **Mac goes to sleep.** Daemon is suspended; CYD shows "host asleep" pill in status bar, last-known data desaturated. Recovers automatically on wake.
- **WiFi drops.** CYD shows "no WiFi", retries every 10 s, then enters AP mode after 5 min of no connection (so the user can move to a new network).
- **Daemon version mismatch with firmware.** The `schema` field in `/v1/stats` is checked. If incompatible, CYD shows "update needed" with the version number. Daemon offers OTA from a future phase.
- **JSONL corruption.** Parser skips malformed records, logs them, never panics. Stats are eventually-consistent.
- **No data yet** (fresh Claude install). All screens render with empty-state copy, not zeroed numbers.

## Testing

| Layer | Approach |
|---|---|
| Daemon parser | Table-driven Go tests with fixture JSONL files capturing known edge cases (gaps, model changes, partial messages). |
| Daemon API | HTTP integration tests hitting `/v1/stats` against fixtures. Snapshot the JSON. |
| Daemon mDNS + pairing | Integration test with two daemon instances, verify pairing token isolation. |
| Firmware logic | C++ unit tests for non-Arduino code (JSON parsing, state machine) via PlatformIO's native env. |
| Firmware UI | Manual on-device verification + LVGL simulator builds for CI screenshot diffs. |
| End-to-end | A `scripts/e2e.sh` that spins up the daemon with fixture data, flashes a connected CYD, and walks through pairing + screen swipe via simulated touch input. |

We use the LVGL PC simulator to render all six screens to PNG in CI, so visual regressions are caught without hardware.

## Open Questions

These don't block the MVP but should be answered before phase 2 ships:

1. **Claude.ai auth.** Cookie paste is fragile (expires). Is there a less hostile option? OAuth via the existing `claude` CLI? Worth investigating.
2. **OTA updates.** Push firmware via the daemon, or have the CYD pull from `cydmonitor.dev`? Daemon-pushed is more reliable on home networks but requires us to host firmware bundles.
3. **Multiple CYDs.** If a user wants one in the office and one at home, do they pair both to the same daemon (LAN-only — they don't) or do we eventually need a small relay? Defer.
4. **Routines API stability.** We shell out to `claude routines list --json` — is that CLI surface stable enough to depend on? If not, parse the underlying state directly.

## What "done" looks like for v1

A user buys a CYD off Amazon, plugs it into their Mac, visits a website to flash it, runs one brew command, taps a code on the CYD, and within five minutes has a glanceable usage display on their desk. They never touched PlatformIO, never opened a serial console, never edited a config file.
