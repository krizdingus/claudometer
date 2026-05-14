# Firmware: USB Provisioning Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the CYD's captive-portal WiFi flow and 4-digit pairing-code flow with a one-shot USB-serial provisioning protocol. The desktop app (next plan) flashes the firmware then immediately pushes WiFi credentials and a bearer token over USB; firmware stores them in NVS and reboots into normal polling.

**Architecture:** A new `usb_provisioner` module owns the serial JSON exchange. The state machine collapses to three reachable states (BOOT, PROVISION, POLL_RENDER) plus DISCOVER as an mDNS fallback when the daemon's IP changes. NVS gains explicit `server_host` and `server_port` keys so the bearer URL is fully resolved from persistent storage. Three modules (`wifi_onboarding`, `pairing_client`, `pair_screen`) plus the `WiFiManager` library dep are deleted.

**Tech Stack:** Arduino-ESP32 + PlatformIO, ArduinoJson 7, LVGL 9, Unity (native tests). Existing patterns: `#ifndef UNIT_TEST` guards on Arduino-only code, pure-C++ headers buildable in native env (`test_build_src = yes`).

---

## Spec section covered

This plan implements the **Firmware changes** section of `docs/superpowers/specs/2026-05-13-desktop-app-design.md` (lines under "Firmware changes" and the relevant parts of "Data flow: first-run provisioning" up to the `wait for "OK"` step). The desktop-app side is a separate plan written after this one lands.

## What success looks like

After this plan:

1. Fresh firmware boots, finds NVS empty, enters PROVISION mode, prints `READY <mac>\n` on serial, blocks reading.
2. Any host (eventually the desktop app; for now `pyserial` or `screen`) can send a single JSON line and the CYD ACKs `OK\n`, writes NVS, reboots.
3. On reboot, NVS has wifi + server_host:port + bearer_token; firmware connects to WiFi and starts polling the existing daemon at the provisioned URL.
4. mDNS still works as a fallback: if the saved `server_host` becomes unreachable, firmware falls back to mDNS discovery and updates NVS with the new IP.
5. All native unit tests pass (existing + new). Firmware builds for `esp32dev`. Flash size stays under 100%.

You can verify end-to-end against the existing Go daemon by minting a token by hand into `~/.config/cydmonitor/pairings.json` and sending the matching provisioning JSON over `screen`. The desktop app is **not** required to validate this plan.

## File structure

**Created:**
- `firmware/src/net/usb_provisioner.h` — declares `ProvisioningCreds`, `parse_provisioning_json` (pure), `run_usb_provisioning` (Arduino).
- `firmware/src/net/usb_provisioner.cpp` — implementation. Pure parser always compiled; `run_usb_provisioning` body wrapped in `#ifndef UNIT_TEST`.
- `firmware/test/native/test_usb_provisioner/test_main.cpp` — Unity tests for the parser.

**Modified:**
- `firmware/src/hw/nvs.h` — add `server_host()`, `server_port()`, `bearer_token()`, `save_server(host, port)`, `save_bearer_token(token)`, `has_server()`. Remove `has_token()`/`token()`/`save_token()`/`daemon_host()`/`save_daemon_host()` (their roles are split across the new methods).
- `firmware/src/hw/nvs.cpp` — implement the new methods; remove the old ones.
- `firmware/src/app/state_machine.h` — remove `State::PAIR` and `Event::PAIR_CONFIRMED`/`PAIR_FAILED` from the enums. `Context` keeps `have_wifi_creds` and `have_token`; reinterpret `have_token` as "have all post-provision fields (wifi + server + token)."
- `firmware/src/app/state_machine.cpp` — drop the PAIR branches; simplify BOOT to two outcomes.
- `firmware/test/native/test_state_machine/test_main.cpp` — replace the four tests that exercise PAIR with tests for the new boot transitions.
- `firmware/src/app/app_loop.cpp` — remove includes/instantiations of `WifiOnboarding`, `PairingClient`, `PairScreen`. Replace `perform_provision` body with `perform_provision_usb`. Remove `perform_pair` entirely. Update `app_init` to build URL from NVS server_host:server_port. Update render logic so the pre-pairing layer only has two children (provision, discover) instead of three.
- `firmware/src/ui/provision_screen.h` — change `set_ap_ssid(const char *)` to `set_status(const char *)` (or add the new method and drop the old).
- `firmware/src/ui/provision_screen.cpp` — update label text to reflect USB flow ("Setup: connect via USB" + a hint to open the desktop app); accept status updates from the provisioner.
- `firmware/platformio.ini` — remove `tzapu/WiFiManager@^2.0.17` from `lib_deps`.

**Deleted:**
- `firmware/src/net/wifi_onboarding.h`
- `firmware/src/net/wifi_onboarding.cpp`
- `firmware/src/net/pairing_client.h`
- `firmware/src/net/pairing_client.cpp`
- `firmware/src/ui/pair_screen.h`
- `firmware/src/ui/pair_screen.cpp`

---

### Task 1: Extend NVS interface for server_host, server_port, bearer_token

**Files:**
- Modify: `firmware/src/hw/nvs.h`
- Modify: `firmware/src/hw/nvs.cpp`

NVS in this project is untested at unit level (no `test_nvs` directory exists, by design — NVS requires the ESP32 runtime). Follow that pattern: extend the interface, implement against `Preferences`, verify on device. The new schema:

| Key (≤15 chars) | Type | Purpose |
|---|---|---|
| `wifi_ssid` | string | (unchanged) |
| `wifi_psk` | string | (unchanged) |
| `srv_host` | string | hostname or IP, no port, no scheme |
| `srv_port` | uint32 | TCP port (default 7842) |
| `bearer` | string | 64-char hex, provisioned over USB |
| `touch_cal` | blob | (unchanged) |

The old `token` and `daemon_host` keys are not migrated — first boot after this upgrade reads them as absent and the firmware will enter PROVISION mode, which is correct.

- [ ] **Step 1: Rewrite `firmware/src/hw/nvs.h`**

```cpp
#pragma once

#include <cstdint>
#include <string>

namespace cyd {

// Thin wrapper over Arduino-ESP32 Preferences. All values live in one NVS
// namespace ("cydmon"). Keys are < 15 chars as required by Preferences.
class Nvs {
 public:
  void begin();

  // WiFi credentials.
  bool has_wifi_creds() const;
  std::string wifi_ssid() const;
  std::string wifi_psk() const;
  void save_wifi(const std::string &ssid, const std::string &psk);

  // Daemon server address (host without scheme/port, plus port).
  bool has_server() const;
  std::string server_host() const;
  uint16_t server_port() const;
  void save_server(const std::string &host, uint16_t port);

  // Bearer token for the daemon (64-char lowercase hex, provisioned over USB).
  bool has_bearer_token() const;
  std::string bearer_token() const;
  void save_bearer_token(const std::string &token);

  // Touch calibration (resistive only). Returns true if cal values were stored.
  bool has_touch_cal() const;
  void load_touch_cal(uint16_t cal[8]) const;
  void save_touch_cal(const uint16_t cal[8]);

  // Wipe all keys, return to factory state.
  void factory_reset();
};

} // namespace cyd
```

- [ ] **Step 2: Rewrite `firmware/src/hw/nvs.cpp`**

Drop the `token()`/`save_token()`/`has_token()`/`daemon_host()`/`save_daemon_host()` methods. Add the new methods. Implementation pattern matches the existing wifi/token implementation; values are stored under the `cydmon` namespace.

```cpp
#include "hw/nvs.h"

#ifndef UNIT_TEST

#include <Preferences.h>

namespace cyd {

namespace {
constexpr const char *kNs = "cydmon";
}

static Preferences prefs;

void Nvs::begin() {
  prefs.begin(kNs, /*readOnly=*/false);
}

bool Nvs::has_wifi_creds() const { return prefs.isKey("wifi_ssid"); }
std::string Nvs::wifi_ssid() const { return prefs.getString("wifi_ssid", "").c_str(); }
std::string Nvs::wifi_psk()  const { return prefs.getString("wifi_psk",  "").c_str(); }
void Nvs::save_wifi(const std::string &ssid, const std::string &psk) {
  prefs.putString("wifi_ssid", ssid.c_str());
  prefs.putString("wifi_psk",  psk.c_str());
}

bool Nvs::has_server() const { return prefs.isKey("srv_host") && prefs.isKey("srv_port"); }
std::string Nvs::server_host() const { return prefs.getString("srv_host", "").c_str(); }
uint16_t Nvs::server_port() const { return (uint16_t)prefs.getUInt("srv_port", 0); }
void Nvs::save_server(const std::string &host, uint16_t port) {
  prefs.putString("srv_host", host.c_str());
  prefs.putUInt("srv_port", port);
}

bool Nvs::has_bearer_token() const { return prefs.isKey("bearer"); }
std::string Nvs::bearer_token() const { return prefs.getString("bearer", "").c_str(); }
void Nvs::save_bearer_token(const std::string &token) {
  prefs.putString("bearer", token.c_str());
}

bool Nvs::has_touch_cal() const { return prefs.isKey("touch_cal"); }
void Nvs::load_touch_cal(uint16_t cal[8]) const {
  prefs.getBytes("touch_cal", cal, sizeof(uint16_t) * 8);
}
void Nvs::save_touch_cal(const uint16_t cal[8]) {
  prefs.putBytes("touch_cal", cal, sizeof(uint16_t) * 8);
}

void Nvs::factory_reset() {
  prefs.clear();
}

} // namespace cyd

#endif // UNIT_TEST
```

- [ ] **Step 3: Confirm the firmware does not yet build**

Run: `cd firmware && pio run -e esp32dev` (path: `/opt/homebrew/bin/pio`)

Expected: FAIL with linker/compile errors in `app_loop.cpp` complaining about removed methods `token()`, `save_token()`, `has_token()`, `daemon_host()`, `save_daemon_host()`. That confirms the interface change took. We'll fix `app_loop.cpp` in Task 5.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/hw/nvs.h firmware/src/hw/nvs.cpp
git commit -m "firmware/nvs: split daemon_host into server_host+port, rename token to bearer"
```

---

### Task 2: Add the pure JSON parser for the provisioning protocol

**Files:**
- Create: `firmware/src/net/usb_provisioner.h`
- Create: `firmware/src/net/usb_provisioner.cpp` (parser portion only in this task)
- Create: `firmware/test/native/test_usb_provisioner/test_main.cpp`

The parser is the testable core. The Arduino-side `run_usb_provisioning` function comes in Task 3. Both live in the same `.cpp` so the Arduino piece can call the parser directly.

The provisioning JSON, sent as a single line:

```json
{"wifi_ssid":"…","wifi_password":"…","server_host":"192.168.1.42","server_port":7842,"bearer_token":"…","provision_schema":1}
```

Field-level rules:
- All six fields required.
- `provision_schema` must equal `1`.
- `wifi_ssid`, `wifi_password`, `server_host`, `bearer_token` are non-empty strings.
- `server_port` is an integer in [1, 65535].

- [ ] **Step 1: Write the failing tests at `firmware/test/native/test_usb_provisioner/test_main.cpp`**

```cpp
#include <unity.h>
#include <string>

#include "net/usb_provisioner.h"

using namespace cyd;

void test_parse_valid_json_populates_all_fields(void) {
  ProvisioningCreds out;
  std::string err;
  bool ok = parse_provisioning_json(
      R"({"wifi_ssid":"Home","wifi_password":"hunter2","server_host":"192.168.1.42","server_port":7842,"bearer_token":"abc123","provision_schema":1})",
      out, err);
  TEST_ASSERT_TRUE_MESSAGE(ok, err.c_str());
  TEST_ASSERT_EQUAL_STRING("Home", out.wifi_ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("hunter2", out.wifi_password.c_str());
  TEST_ASSERT_EQUAL_STRING("192.168.1.42", out.server_host.c_str());
  TEST_ASSERT_EQUAL(7842, out.server_port);
  TEST_ASSERT_EQUAL_STRING("abc123", out.bearer_token.c_str());
}

void test_parse_rejects_missing_field(void) {
  ProvisioningCreds out;
  std::string err;
  // missing bearer_token
  bool ok = parse_provisioning_json(
      R"({"wifi_ssid":"H","wifi_password":"p","server_host":"h","server_port":7842,"provision_schema":1})",
      out, err);
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_TRUE(err.find("bearer_token") != std::string::npos);
}

void test_parse_rejects_wrong_schema(void) {
  ProvisioningCreds out;
  std::string err;
  bool ok = parse_provisioning_json(
      R"({"wifi_ssid":"H","wifi_password":"p","server_host":"h","server_port":7842,"bearer_token":"t","provision_schema":2})",
      out, err);
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_TRUE(err.find("schema") != std::string::npos);
}

void test_parse_rejects_empty_string_field(void) {
  ProvisioningCreds out;
  std::string err;
  bool ok = parse_provisioning_json(
      R"({"wifi_ssid":"","wifi_password":"p","server_host":"h","server_port":7842,"bearer_token":"t","provision_schema":1})",
      out, err);
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_TRUE(err.find("wifi_ssid") != std::string::npos);
}

void test_parse_rejects_port_out_of_range(void) {
  ProvisioningCreds out;
  std::string err;
  bool ok = parse_provisioning_json(
      R"({"wifi_ssid":"H","wifi_password":"p","server_host":"h","server_port":0,"bearer_token":"t","provision_schema":1})",
      out, err);
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_TRUE(err.find("server_port") != std::string::npos);
}

void test_parse_rejects_malformed_json(void) {
  ProvisioningCreds out;
  std::string err;
  bool ok = parse_provisioning_json("not json at all", out, err);
  TEST_ASSERT_FALSE(ok);
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_parse_valid_json_populates_all_fields);
  RUN_TEST(test_parse_rejects_missing_field);
  RUN_TEST(test_parse_rejects_wrong_schema);
  RUN_TEST(test_parse_rejects_empty_string_field);
  RUN_TEST(test_parse_rejects_port_out_of_range);
  RUN_TEST(test_parse_rejects_malformed_json);
  return UNITY_END();
}
```

- [ ] **Step 2: Run tests to confirm they fail to compile**

Run: `cd firmware && pio test -e native -f test_usb_provisioner`

Expected: build fails because `net/usb_provisioner.h` does not exist yet.

- [ ] **Step 3: Write `firmware/src/net/usb_provisioner.h`**

```cpp
#pragma once

#include <cstdint>
#include <string>

namespace cyd {

struct ProvisioningCreds {
  std::string wifi_ssid;
  std::string wifi_password;
  std::string server_host;
  uint16_t server_port = 0;
  std::string bearer_token;
};

// Pure function: parse one JSON line into ProvisioningCreds. Returns true on
// success. On failure, populates err with a short reason that names the
// offending field (so the host can show a useful error to the user).
bool parse_provisioning_json(const std::string &json_line,
                             ProvisioningCreds &out,
                             std::string &err);

// Arduino-only: blocks reading from Serial, prints READY <mac>, reads one line
// of JSON, parses it. On parse failure prints "ERR <reason>" and returns false
// (caller can retry). On parse success returns true and prints "OK" — caller
// is expected to write to NVS and reboot.
bool run_usb_provisioning(ProvisioningCreds &out);

} // namespace cyd
```

- [ ] **Step 4: Write the parser in `firmware/src/net/usb_provisioner.cpp`**

ArduinoJson 7's `JsonDocument` works on the native env too (declared as a `lib_deps` entry for `[env:native]` already). The parser is fully portable.

```cpp
#include "net/usb_provisioner.h"

#include <ArduinoJson.h>

namespace cyd {

namespace {

bool require_nonempty_string(JsonDocument &doc, const char *key,
                             std::string &out, std::string &err) {
  if (!doc[key].is<const char *>()) {
    err = std::string("missing or non-string: ") + key;
    return false;
  }
  out = doc[key].as<const char *>();
  if (out.empty()) {
    err = std::string("empty: ") + key;
    return false;
  }
  return true;
}

} // namespace

bool parse_provisioning_json(const std::string &json_line,
                             ProvisioningCreds &out,
                             std::string &err) {
  JsonDocument doc;
  DeserializationError de = deserializeJson(doc, json_line);
  if (de) {
    err = std::string("json: ") + de.c_str();
    return false;
  }

  if (!doc["provision_schema"].is<int>() || doc["provision_schema"].as<int>() != 1) {
    err = "unsupported provision_schema";
    return false;
  }

  if (!require_nonempty_string(doc, "wifi_ssid",     out.wifi_ssid,     err)) return false;
  if (!require_nonempty_string(doc, "wifi_password", out.wifi_password, err)) return false;
  if (!require_nonempty_string(doc, "server_host",   out.server_host,   err)) return false;
  if (!require_nonempty_string(doc, "bearer_token",  out.bearer_token,  err)) return false;

  if (!doc["server_port"].is<int>()) {
    err = "missing or non-int: server_port";
    return false;
  }
  int port = doc["server_port"].as<int>();
  if (port <= 0 || port > 65535) {
    err = "server_port out of range";
    return false;
  }
  out.server_port = (uint16_t)port;

  return true;
}

#ifndef UNIT_TEST

// Arduino-side body lives here but is defined in Task 3.

#endif // UNIT_TEST

} // namespace cyd
```

- [ ] **Step 5: Run tests to confirm they pass**

Run: `cd firmware && pio test -e native -f test_usb_provisioner`

Expected: 6 tests pass. If any fails, fix the parser and re-run.

- [ ] **Step 6: Run the full native test suite to confirm nothing else broke**

Run: `cd firmware && pio test -e native`

Expected: existing state_machine and stats_parser tests still pass (they will, this task didn't touch them).

- [ ] **Step 7: Commit**

```bash
git add firmware/src/net/usb_provisioner.h firmware/src/net/usb_provisioner.cpp firmware/test/native/test_usb_provisioner/
git commit -m "firmware/usb_provisioner: parse JSON provisioning payload"
```

---

### Task 3: Add the Arduino-side serial protocol on top of the parser

**Files:**
- Modify: `firmware/src/net/usb_provisioner.cpp`

The Arduino-side function:

1. Prints `READY <mac>\n` once.
2. Loops: read a line from `Serial` (newline-terminated, up to 1024 bytes).
3. Hand the line to `parse_provisioning_json`.
4. On failure → `Serial.print("ERR <reason>\n")`, continue.
5. On success → `Serial.print("OK\n")`, return true.

The function never returns on its own — only when valid JSON arrives. Caller is responsible for rebooting afterward. There's no timeout; the firmware just sits at "PROVISION" forever until a host talks to it.

The MAC in the READY message is the ESP32's WiFi station MAC, colon-formatted (e.g. `AA:BB:CC:DD:EE:FF`). The desktop app will use this as the stable `device_id`.

- [ ] **Step 1: Add the Arduino-side implementation to `firmware/src/net/usb_provisioner.cpp`**

Replace the empty `#ifndef UNIT_TEST` block from Task 2 with:

```cpp
#ifndef UNIT_TEST

#include <Arduino.h>
#include <WiFi.h>

namespace {

std::string read_line(uint32_t max_bytes = 1024) {
  std::string line;
  line.reserve(256);
  while (line.size() < max_bytes) {
    while (!Serial.available()) {
      delay(10);
    }
    int b = Serial.read();
    if (b < 0) continue;
    if (b == '\n') return line;
    if (b == '\r') continue;
    line.push_back((char)b);
  }
  return line;
}

} // namespace

bool run_usb_provisioning(ProvisioningCreds &out) {
  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);
  Serial.printf("READY %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  for (;;) {
    std::string line = read_line();
    if (line.empty()) continue;

    std::string err;
    if (parse_provisioning_json(line, out, err)) {
      Serial.print("OK\n");
      return true;
    }
    Serial.printf("ERR %s\n", err.c_str());
  }
}

#endif // UNIT_TEST
```

- [ ] **Step 2: Confirm the firmware now compiles past the provisioner (other failures expected)**

Run: `cd firmware && pio run -e esp32dev`

Expected: still fails, but the errors should now be confined to `app_loop.cpp` (Task 5). The provisioner itself compiles cleanly.

- [ ] **Step 3: Commit**

```bash
git add firmware/src/net/usb_provisioner.cpp
git commit -m "firmware/usb_provisioner: add Arduino-side serial protocol"
```

---

### Task 4: Simplify the state machine

**Files:**
- Modify: `firmware/src/app/state_machine.h`
- Modify: `firmware/src/app/state_machine.cpp`
- Modify: `firmware/test/native/test_state_machine/test_main.cpp`

The new state graph:

```
BOOT ─── all NVS present? ── yes ──► POLL_RENDER
                          ── no  ──► PROVISION ── WIFI_OK ──► (reboot, re-enter BOOT)

POLL_RENDER ── DAEMON_UNREACHABLE ──► DISCOVER ── DAEMON_FOUND ──► POLL_RENDER
DISCOVER    ── DAEMON_NOT_FOUND   ──► POLL_RENDER (stays stale)

Any state ── FACTORY_RESET ──► PROVISION
```

`PAIR` is gone. The `Context::have_token` flag now means "has bearer_token AND server_host AND server_port" (i.e., everything provisioned over USB). The app_loop in Task 5 sets it that way.

- [ ] **Step 1: Update `firmware/src/app/state_machine.h`**

```cpp
#pragma once

namespace cyd {

enum class State {
  BOOT,
  PROVISION,    // USB serial mode, awaiting provisioning JSON
  DISCOVER,     // WiFi up, scanning mDNS for daemon (fallback path)
  POLL_RENDER,  // steady state: poll /v1/stats, draw screens
};

enum class Event {
  TICK,
  WIFI_OK,
  WIFI_FAIL,
  DAEMON_FOUND,
  DAEMON_NOT_FOUND,
  DAEMON_UNREACHABLE,
  DAEMON_RECOVERED,
  FACTORY_RESET,
};

struct Context {
  bool have_wifi_creds = false;
  bool have_token = false;  // have full provisioning bundle (server + bearer)
};

// next_state is a pure function: given current state, an event, and the
// persistence context, return the state to transition to. Hardware effects
// (showing screens, opening sockets) are the caller's responsibility.
State next_state(State current, Event event, const Context &ctx);

} // namespace cyd
```

- [ ] **Step 2: Update `firmware/src/app/state_machine.cpp`**

```cpp
#include "app/state_machine.h"

namespace cyd {

State next_state(State current, Event event, const Context &ctx) {
  if (event == Event::FACTORY_RESET) return State::PROVISION;

  switch (current) {
    case State::BOOT:
      if (!ctx.have_wifi_creds || !ctx.have_token) return State::PROVISION;
      return State::POLL_RENDER;

    case State::PROVISION:
      // PROVISION only exits via reboot after writing NVS, so any event other
      // than FACTORY_RESET keeps us here. We still handle WIFI_OK for symmetry.
      if (event == Event::WIFI_OK) return State::POLL_RENDER;
      return State::PROVISION;

    case State::DISCOVER:
      if (event == Event::DAEMON_FOUND) return State::POLL_RENDER;
      if (event == Event::DAEMON_NOT_FOUND) return State::POLL_RENDER;
      if (event == Event::WIFI_FAIL) return State::PROVISION;
      return State::DISCOVER;

    case State::POLL_RENDER:
      if (event == Event::DAEMON_UNREACHABLE) return State::DISCOVER;
      return State::POLL_RENDER;
  }
  return current;
}

} // namespace cyd
```

- [ ] **Step 3: Rewrite `firmware/test/native/test_state_machine/test_main.cpp`**

```cpp
#include <unity.h>

#include "app/state_machine.h"

using namespace cyd;

void test_boot_with_no_creds_goes_to_provision(void) {
  Context ctx{};
  ctx.have_wifi_creds = false;
  ctx.have_token = false;
  TEST_ASSERT_EQUAL(State::PROVISION, next_state(State::BOOT, Event::TICK, ctx));
}

void test_boot_with_wifi_but_no_token_goes_to_provision(void) {
  // No PAIR state anymore — partial NVS goes back through PROVISION.
  Context ctx{};
  ctx.have_wifi_creds = true;
  ctx.have_token = false;
  TEST_ASSERT_EQUAL(State::PROVISION, next_state(State::BOOT, Event::TICK, ctx));
}

void test_boot_with_full_credentials_goes_to_poll(void) {
  Context ctx{};
  ctx.have_wifi_creds = true;
  ctx.have_token = true;
  TEST_ASSERT_EQUAL(State::POLL_RENDER, next_state(State::BOOT, Event::TICK, ctx));
}

void test_provision_holds_until_wifi_ok(void) {
  Context ctx{};
  TEST_ASSERT_EQUAL(State::PROVISION, next_state(State::PROVISION, Event::TICK, ctx));
  TEST_ASSERT_EQUAL(State::POLL_RENDER, next_state(State::PROVISION, Event::WIFI_OK, ctx));
}

void test_poll_to_discover_on_unreachable(void) {
  Context ctx{};
  TEST_ASSERT_EQUAL(State::DISCOVER,
                    next_state(State::POLL_RENDER, Event::DAEMON_UNREACHABLE, ctx));
}

void test_discover_to_poll_on_found_or_not_found(void) {
  Context ctx{};
  TEST_ASSERT_EQUAL(State::POLL_RENDER,
                    next_state(State::DISCOVER, Event::DAEMON_FOUND, ctx));
  TEST_ASSERT_EQUAL(State::POLL_RENDER,
                    next_state(State::DISCOVER, Event::DAEMON_NOT_FOUND, ctx));
}

void test_factory_reset_returns_to_provision(void) {
  Context ctx{};
  TEST_ASSERT_EQUAL(State::PROVISION,
                    next_state(State::POLL_RENDER, Event::FACTORY_RESET, ctx));
  TEST_ASSERT_EQUAL(State::PROVISION,
                    next_state(State::DISCOVER, Event::FACTORY_RESET, ctx));
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_boot_with_no_creds_goes_to_provision);
  RUN_TEST(test_boot_with_wifi_but_no_token_goes_to_provision);
  RUN_TEST(test_boot_with_full_credentials_goes_to_poll);
  RUN_TEST(test_provision_holds_until_wifi_ok);
  RUN_TEST(test_poll_to_discover_on_unreachable);
  RUN_TEST(test_discover_to_poll_on_found_or_not_found);
  RUN_TEST(test_factory_reset_returns_to_provision);
  return UNITY_END();
}
```

- [ ] **Step 4: Run tests**

Run: `cd firmware && pio test -e native -f test_state_machine`

Expected: 7 tests pass.

- [ ] **Step 5: Run all native tests**

Run: `cd firmware && pio test -e native`

Expected: state_machine (7) + stats_parser (existing) + usb_provisioner (6) all pass.

- [ ] **Step 6: Commit**

```bash
git add firmware/src/app/state_machine.h firmware/src/app/state_machine.cpp firmware/test/native/test_state_machine/
git commit -m "firmware/state_machine: collapse PAIR into PROVISION, simplify transitions"
```

---

### Task 5: Rewrite app_loop boot flow and remove pair/portal code paths

**Files:**
- Modify: `firmware/src/app/app_loop.cpp`

This is the big surgical task. Net change: remove three `#include`s, remove three member pointers (`wifi_`, `pairing_`, `pair_`), remove three functions (`perform_provision` as captive portal, `perform_pair`, `perform_discover` saved-host shortcut — folded into boot), add one (`perform_provision_usb`), and rewrite `app_init`.

The new boot rule:

1. `nvs_->begin()`.
2. `ctx_.have_wifi_creds = nvs_->has_wifi_creds()`.
3. `ctx_.have_token = nvs_->has_server() && nvs_->has_bearer_token()`.
4. If both true → construct `daemon_base_url_` from `nvs_->server_host()` + `nvs_->server_port()` and try `WiFi.begin(saved ssid, saved psk)`. If WiFi connects within 12s → `apply_event(WIFI_OK)` (BOOT→POLL_RENDER). If WiFi fails → tear down, fall through to PROVISION.
5. If either is false → `apply_event(TICK)` enters PROVISION; the next call to `perform_provision_usb()` handles the rest.

The pre-pairing layer now has two children instead of three (provision, discover). The `pair_screen` is gone. Update `render_state` accordingly.

- [ ] **Step 1: Rewrite `firmware/src/app/app_loop.cpp`**

Below is the full new content. The changes from the existing file:
- Remove includes for `net/pairing_client.h`, `net/wifi_onboarding.h`, `ui/pair_screen.h`.
- Add include for `net/usb_provisioner.h`.
- Remove the `wifi_`, `pairing_`, `pair_`, `pending_code_`, `confirm_pressed_`, `on_pair_confirm`, `device_cyd_id()`, `device_name()` symbols.
- Remove `perform_provision`, `perform_pair`, the saved-host shortcut in `perform_discover`.
- Add `perform_provision_usb`.
- Rewrite `app_init` boot path.
- Rewrite `render_state` for two pre-pairing children.

```cpp
#include "app/app_loop.h"

#ifndef UNIT_TEST

#include <Arduino.h>
#include <WiFi.h>
#include <algorithm>
#include <lvgl.h>

#include "app/app_config.h"
#include "app/long_press.h"
#include "app/state_machine.h"
#include "hw/display.h"
#include "hw/nvs.h"
#include "hw/touch.h"
#include "net/mdns_discover.h"
#include "net/stats_client.h"
#include "net/usb_provisioner.h"
#include "ui/chrome.h"
#include "ui/discover_screen.h"
#include "ui/lvgl_glue.h"
#include "ui/provision_screen.h"
#include "ui/screen_budgets.h"
#include "ui/screen_home.h"
#include "ui/screen_models.h"
#include "ui/screen_routines.h"
#include "ui/screen_session.h"
#include "ui/screen_sonnet.h"
#include "ui/theme.h"
#include "ui/tileview.h"

namespace cyd {

namespace {

State current_state = State::BOOT;
Context ctx_;
Nvs *nvs_ = nullptr;
Chrome *chrome_ = nullptr;
Tileview *tileview_ = nullptr;
ScreenHome *scr_home_ = nullptr;
ScreenSession *scr_session_ = nullptr;
ScreenModels *scr_models_ = nullptr;
ScreenSonnet *scr_sonnet_ = nullptr;
ScreenRoutines *scr_routines_ = nullptr;
ScreenBudgets *scr_budgets_ = nullptr;
ProvisionScreen *prov_ = nullptr;
DiscoverScreen *disc_ = nullptr;
MdnsDiscover *mdns_ = nullptr;
StatsClient *stats_client_ = nullptr;
Stats last_stats_;
std::string daemon_base_url_;
std::string daemon_display_;
uint32_t next_poll_at_ = 0;
uint32_t backoff_ms_ = kBackoffStartMs;

lv_obj_t *root_ = nullptr;
lv_obj_t *pre_pairing_layer_ = nullptr;   // hosts prov + disc
lv_obj_t *main_layer_ = nullptr;          // hosts tileview + chrome

void show_layer(lv_obj_t *layer) {
  lv_obj_add_flag(pre_pairing_layer_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(main_layer_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(layer, LV_OBJ_FLAG_HIDDEN);
}

void render_state(State s) {
  if (s == State::PROVISION || s == State::DISCOVER) {
    show_layer(pre_pairing_layer_);
    // Pre-pairing children: 0=prov, 1=disc.
    if (s == State::PROVISION) {
      lv_obj_clear_flag(lv_obj_get_child(pre_pairing_layer_, 0), LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(lv_obj_get_child(pre_pairing_layer_, 1), LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(lv_obj_get_child(pre_pairing_layer_, 0), LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(lv_obj_get_child(pre_pairing_layer_, 1), LV_OBJ_FLAG_HIDDEN);
    }
  } else {
    show_layer(main_layer_);
  }
}

void apply_event(Event e) {
  State next = next_state(current_state, e, ctx_);
  if (next != current_state) {
    Serial.printf("state: %d -> %d\n", (int)current_state, (int)next);
    current_state = next;
    render_state(current_state);
  }
}

void on_screen_change(Screen s) {
  if (chrome_) chrome_->set_active_screen(s);
  next_poll_at_ = 0;
}

void update_all_screens(const Stats &s) {
  if (scr_home_) scr_home_->update(s);
  if (scr_session_) scr_session_->update(s);
  if (scr_models_) scr_models_->update(s);
  if (scr_sonnet_) scr_sonnet_->update(s);
  if (scr_routines_) scr_routines_->update(s);
  if (scr_budgets_) scr_budgets_->update(s);
  if (chrome_) {
    chrome_->set_health(s.stale ? 1 : 0);
    if (!s.local_time.empty()) chrome_->set_clock(s.local_time.c_str());
  }
}

void perform_provision_usb() {
  // Block here until a host sends a valid provisioning JSON.
  ProvisioningCreds creds;
  if (!run_usb_provisioning(creds)) return;  // shouldn't happen — it loops

  // Persist to NVS and reboot. On next boot the new state will short-circuit
  // BOOT → POLL_RENDER.
  if (nvs_) {
    nvs_->save_wifi(creds.wifi_ssid, creds.wifi_password);
    nvs_->save_server(creds.server_host, creds.server_port);
    nvs_->save_bearer_token(creds.bearer_token);
  }
  Serial.println("provisioned, restarting");
  delay(200);
  ESP.restart();
}

void perform_discover() {
  // mDNS fallback when a saved server_host stops responding.
  DaemonAddr addr;
  if (!mdns_ || !mdns_->find(addr)) {
    delay(kMdnsQueryMs);
    apply_event(Event::DAEMON_NOT_FOUND);
    return;
  }
  char hp[96];
  snprintf(hp, sizeof(hp), "%s:%u", addr.hostname.c_str(), addr.port);
  daemon_base_url_ = std::string("http://") + hp;
  daemon_display_ = addr.display;
  if (nvs_) nvs_->save_server(addr.hostname, addr.port);
  apply_event(Event::DAEMON_FOUND);
}

void perform_poll() {
  uint32_t now = millis();
  if (now < next_poll_at_) return;

  Stats fresh;
  std::string token = nvs_ ? nvs_->bearer_token() : "";
  uint8_t mask = tileview_ ? tileview_->neighbor_mask() : 0;
  bool ok = stats_client_ && stats_client_->fetch(daemon_base_url_, token, mask, fresh);
  Serial.printf("poll url=%s mask=0x%02X ok=%d total=%d sess=%d%%\n",
                daemon_base_url_.c_str(), mask, ok ? 1 : 0,
                fresh.models_today.total_tokens, fresh.session.pct_used);
  if (ok) {
    last_stats_ = fresh;
    last_stats_.stale = false;
    update_all_screens(last_stats_);
    backoff_ms_ = kBackoffStartMs;
    bool active_is_hot = (tileview_ &&
                          (tileview_->active() == SCR_SESSION ||
                           tileview_->active() == SCR_MODELS));
    next_poll_at_ = now + (active_is_hot ? kActivePollMs : kIdlePollMs);
    apply_event(Event::DAEMON_RECOVERED);
  } else {
    last_stats_.stale = true;
    update_all_screens(last_stats_);
    next_poll_at_ = now + backoff_ms_;
    backoff_ms_ = std::min<uint32_t>(backoff_ms_ * 2, kBackoffMaxMs);
    if (chrome_) chrome_->set_health(2);
    apply_event(Event::DAEMON_UNREACHABLE);
  }
}

bool try_connect_saved_wifi() {
  if (!nvs_ || !nvs_->has_wifi_creds()) return false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(nvs_->wifi_ssid().c_str(), nvs_->wifi_psk().c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 12000) return false;
    delay(200);
  }
  return true;
}

} // namespace

void app_init() {
  display().init();
  display().setRotation(0);
  display().setBrightness(200);
  touch().probe_and_init();
  lvgl_init();

  nvs_ = new Nvs();
  nvs_->begin();

  ctx_.have_wifi_creds = nvs_->has_wifi_creds();
  ctx_.have_token      = nvs_->has_server() && nvs_->has_bearer_token();

  if (ctx_.have_token) {
    char hp[96];
    snprintf(hp, sizeof(hp), "%s:%u",
             nvs_->server_host().c_str(), nvs_->server_port());
    daemon_base_url_ = std::string("http://") + hp;
    daemon_display_ = hp;
  }

  root_ = lv_screen_active();
  lv_obj_set_style_bg_color(root_, theme::c(theme::bg), 0);

  chrome_ = new Chrome();
  chrome_->attach(root_);

  pre_pairing_layer_ = lv_obj_create(root_);
  lv_obj_set_size(pre_pairing_layer_, 240, 320 - kStatusBarHeight - kFooterHeight);
  lv_obj_align(pre_pairing_layer_, LV_ALIGN_TOP_MID, 0, kStatusBarHeight);
  lv_obj_set_style_bg_color(pre_pairing_layer_, theme::c(theme::bg), 0);
  lv_obj_set_style_border_width(pre_pairing_layer_, 0, 0);
  lv_obj_set_style_radius(pre_pairing_layer_, 0, 0);
  lv_obj_set_style_pad_all(pre_pairing_layer_, 0, 0);
  lv_obj_clear_flag(pre_pairing_layer_, LV_OBJ_FLAG_SCROLLABLE);
  // Children: 0=prov, 1=disc (referenced by index in render_state).
  prov_ = new ProvisionScreen();
  disc_ = new DiscoverScreen();
  prov_->build(lv_obj_create(pre_pairing_layer_));
  disc_->build(lv_obj_create(pre_pairing_layer_));

  main_layer_ = lv_obj_create(root_);
  lv_obj_set_size(main_layer_, 240, 320 - kStatusBarHeight - kFooterHeight);
  lv_obj_align(main_layer_, LV_ALIGN_TOP_MID, 0, kStatusBarHeight);
  lv_obj_set_style_bg_color(main_layer_, theme::c(theme::bg), 0);
  lv_obj_set_style_border_width(main_layer_, 0, 0);
  lv_obj_set_style_radius(main_layer_, 0, 0);
  lv_obj_set_style_pad_all(main_layer_, 0, 0);
  lv_obj_clear_flag(main_layer_, LV_OBJ_FLAG_SCROLLABLE);

  tileview_ = new Tileview();
  tileview_->attach(main_layer_);
  tileview_->on_change(&on_screen_change);
  scr_home_ = new ScreenHome();
  scr_session_ = new ScreenSession();
  scr_models_ = new ScreenModels();
  scr_sonnet_ = new ScreenSonnet();
  scr_routines_ = new ScreenRoutines();
  scr_budgets_ = new ScreenBudgets();
  scr_home_->build(tileview_->tile(SCR_HOME));
  scr_session_->build(tileview_->tile(SCR_SESSION));
  scr_models_->build(tileview_->tile(SCR_MODELS));
  scr_sonnet_->build(tileview_->tile(SCR_SONNET));
  scr_routines_->build(tileview_->tile(SCR_ROUTINES));
  scr_budgets_->build(tileview_->tile(SCR_BUDGETS));

  mdns_ = new MdnsDiscover();
  stats_client_ = new StatsClient();

  // Initial transition from BOOT — uses ctx_ filled above.
  apply_event(Event::TICK);

  // If we believe we have full credentials, try to connect now. On failure,
  // fall back to PROVISION (USB) and let the user re-run setup.
  if (current_state == State::POLL_RENDER) {
    if (!try_connect_saved_wifi()) {
      current_state = State::PROVISION;
      ctx_.have_wifi_creds = false;
      ctx_.have_token = false;
      render_state(current_state);
    }
  }
}

void app_tick() {
  lvgl_tick();
  static LongPress long_press;
  auto ev = touch().poll();
  if (long_press.update(ev.pressed, millis(), kLongPressMs)) {
    Serial.println("factory reset triggered");
    nvs_->factory_reset();
    ctx_ = Context{};
    apply_event(Event::FACTORY_RESET);
  }
  switch (current_state) {
    case State::PROVISION:  perform_provision_usb(); break;
    case State::DISCOVER:   perform_discover();      break;
    case State::POLL_RENDER:perform_poll();          break;
    default: break;
  }
}

} // namespace cyd

#endif  // UNIT_TEST
```

- [ ] **Step 2: Build the firmware**

Run: `cd firmware && pio run -e esp32dev`

Expected: PASS. Build succeeds. There will be a flash size percentage near or below the current 87.1% — removing WiFiManager + portal + pairing client should free some space. Confirm the build doesn't overflow.

If it fails to build because `provision_screen.h` still has `set_ap_ssid` (a leftover Task 6 dep), continue to Task 6 and try again.

- [ ] **Step 3: Run all native tests**

Run: `cd firmware && pio test -e native`

Expected: all pass (state_machine 7, stats_parser existing, usb_provisioner 6).

- [ ] **Step 4: Commit**

```bash
git add firmware/src/app/app_loop.cpp
git commit -m "firmware/app_loop: drive boot from NVS, replace captive portal with USB provisioning"
```

---

### Task 6: Update provision_screen UI text

**Files:**
- Modify: `firmware/src/ui/provision_screen.h`
- Modify: `firmware/src/ui/provision_screen.cpp`

The provision screen currently displays the AP SSID for the captive portal. The new flow has no AP; it tells the user to connect via USB and open the desktop app. The header should read something like "Setup" with a body like "Connect the CYD to your computer via USB and open CYDMonitor on your desktop."

- [ ] **Step 1: Read the current `provision_screen.h`**

Run: `cat firmware/src/ui/provision_screen.h`

- [ ] **Step 2: Replace `set_ap_ssid(const char *)` with `set_status(const char *)`**

In `provision_screen.h`, change the method signature:

```cpp
// Before:
void set_ap_ssid(const char *ssid);
// After:
void set_status(const char *msg);
```

Keep the same private members; just rename the public method.

- [ ] **Step 3: Update `provision_screen.cpp` to render the new copy**

Replace the dynamic SSID label with static body text. The exact layout depends on what the file currently looks like — read it first, then change:
- Title: "Setup"
- Body line 1: "Connect via USB to your computer"
- Body line 2: "Open CYDMonitor to begin"
- Optional small status line at the bottom that `set_status()` updates (e.g., "Waiting…" by default; the app_loop doesn't currently call set_status, but having the hook keeps the screen flexible).

Remove any AP-mode-specific text (mentions of WiFi network names, joining hotspots, etc.).

- [ ] **Step 4: Build the firmware**

Run: `cd firmware && pio run -e esp32dev`

Expected: PASS.

- [ ] **Step 5: Confirm `set_ap_ssid` is no longer referenced anywhere**

Run: `grep -rn "set_ap_ssid" firmware/`

Expected: no matches (the app_loop call was already removed in Task 5).

- [ ] **Step 6: Commit**

```bash
git add firmware/src/ui/provision_screen.h firmware/src/ui/provision_screen.cpp
git commit -m "firmware/ui: rewrite provision screen for USB-based setup flow"
```

---

### Task 7: Delete dead modules

**Files:**
- Delete: `firmware/src/net/wifi_onboarding.h`
- Delete: `firmware/src/net/wifi_onboarding.cpp`
- Delete: `firmware/src/net/pairing_client.h`
- Delete: `firmware/src/net/pairing_client.cpp`
- Delete: `firmware/src/ui/pair_screen.h`
- Delete: `firmware/src/ui/pair_screen.cpp`

These were the captive portal, the 4-digit code HTTP client, and the 4-digit code display screen. Task 5 already removed the references; the files just sit unused. Delete them and remove the WiFiManager library dep.

- [ ] **Step 1: Confirm no remaining references**

Run: `grep -rn "wifi_onboarding\|pairing_client\|pair_screen\|WiFiManager" firmware/src firmware/test`

Expected: no matches in `src/` or `test/` after Task 5 and Task 6. (Matches inside the to-be-deleted files themselves are fine.)

- [ ] **Step 2: Delete the files**

Run:
```bash
rm firmware/src/net/wifi_onboarding.h \
   firmware/src/net/wifi_onboarding.cpp \
   firmware/src/net/pairing_client.h \
   firmware/src/net/pairing_client.cpp \
   firmware/src/ui/pair_screen.h \
   firmware/src/ui/pair_screen.cpp
```

- [ ] **Step 3: Remove `tzapu/WiFiManager` from `firmware/platformio.ini`**

In the `[env:esp32dev]` section's `lib_deps`, remove the line:

```
  tzapu/WiFiManager@^2.0.17
```

Leave the other three deps (LovyanGFX, lvgl, ArduinoJson). The `[env:native]` section already lacks WiFiManager.

- [ ] **Step 4: Build the firmware to confirm nothing depends on the deleted code**

Run: `cd firmware && pio run -e esp32dev`

Expected: PASS. Flash size should drop noticeably (a few percent) now that WiFiManager and its HTTP server are gone.

- [ ] **Step 5: Run all native tests**

Run: `cd firmware && pio test -e native`

Expected: all pass.

- [ ] **Step 6: Commit**

```bash
git add -u firmware/src/net firmware/src/ui firmware/platformio.ini
git commit -m "firmware: delete wifi_onboarding, pairing_client, pair_screen, WiFiManager dep"
```

---

### Task 8: End-to-end manual test on hardware

**Files:**
- No code changes. Verify-only task.

This task verifies the full new flow against the existing Go daemon — no desktop app needed. We use the existing daemon's pairings.json to seed a bearer token, then send a provisioning JSON to the CYD over USB serial.

- [ ] **Step 1: Wipe NVS on the CYD**

Connect the CYD via USB (port should be `/dev/cu.usbserial-1110` per project memory, but adjust if different).

Run: `cd firmware && pio run -e esp32dev -t erase` (uses esptool's `erase_flash`).

This guarantees we boot into PROVISION with no leftover state.

- [ ] **Step 2: Flash the new firmware**

Run: `cd firmware && pio run -e esp32dev -t upload`

Expected: PASS. Upload completes at 460800 baud.

- [ ] **Step 3: Open a serial monitor and confirm READY**

Run: `pio device monitor -e esp32dev` (or `screen /dev/cu.usbserial-1110 115200`)

Expected: within a second of boot, the firmware prints:
```
state: 0 -> 1
READY AA:BB:CC:DD:EE:FF
```
(with the actual MAC). The CYD display shows the new provision screen ("Setup — Connect via USB…").

- [ ] **Step 4: Start the existing daemon**

In another terminal:

```bash
cd daemon && make build
CYDMONITOR_PLAN=max-5x ./bin/cydmonitor &
```

Expected: daemon serves on :7842.

- [ ] **Step 5: Mint a bearer token by hand into pairings.json**

The daemon expects tokens in `~/.config/cydmonitor/pairings.json`. Create or edit that file to include a known token:

```bash
mkdir -p ~/.config/cydmonitor
TOKEN=$(openssl rand -hex 32)
echo "{\"pairings\":[{\"device_id\":\"manual-test\",\"token\":\"$TOKEN\",\"created_at\":\"2026-05-13T00:00:00Z\"}]}" > ~/.config/cydmonitor/pairings.json
echo "TOKEN=$TOKEN"
```

(If the daemon's pairings.json schema is different from this shape, fall back to running `./bin/cydmonitor` and using the existing pair-init/pair-verify HTTP endpoints once via `curl` to mint a token. The daemon is still backward-compatible for now.)

Run: `./bin/cydmonitor status` to confirm the token is loaded.

- [ ] **Step 6: Send the provisioning JSON over serial**

With the CYD still in PROVISION mode (still printing nothing past READY), send a single JSON line over the same serial port. Quit the monitor first (Ctrl+C in `pio device monitor`).

Find your Mac's LAN IP: `ipconfig getifaddr en0` (or whichever interface is active).

```bash
LAN_IP=$(ipconfig getifaddr en0)
python3 -c "
import serial, json, sys
s = serial.Serial('/dev/cu.usbserial-1110', 115200, timeout=10)
payload = {
  'wifi_ssid':       'YOUR_SSID',
  'wifi_password':   'YOUR_PASSWORD',
  'server_host':     '${LAN_IP}',
  'server_port':     7842,
  'bearer_token':    '${TOKEN}',
  'provision_schema': 1,
}
s.write((json.dumps(payload) + '\n').encode())
while True:
    line = s.readline().decode(errors='replace').rstrip()
    if not line: break
    print(line)
    if line == 'OK' or line.startswith('ERR'): break
"
```

Expected output: `OK`, followed by `provisioned, restarting`, then the device reboots.

- [ ] **Step 7: Reopen the serial monitor and confirm polling starts**

Run: `pio device monitor -e esp32dev`

Expected, within ~15 seconds of restart:
```
state: 0 -> 3            # BOOT -> POLL_RENDER (enum int values)
poll url=http://<LAN_IP>:7842 mask=0x… ok=1 total=… sess=…%
```

CYD display now shows the six normal screens (Home, Session, Models, Sonnet, Routines, Budgets). Swiping works. Numbers match what the daemon serves.

- [ ] **Step 8: Verify factory reset still works**

Long-press the touchscreen for 5+ seconds. Expected: serial prints `factory reset triggered`, NVS wipes, state transitions back to PROVISION, display returns to "Setup — Connect via USB…".

- [ ] **Step 9: Verify mDNS fallback (optional but recommended)**

Provision the device again as in Steps 6–7 but with an intentionally wrong `server_host` (e.g., `192.0.2.1` — TEST-NET-1, guaranteed unreachable). Expected:
- Device transitions BOOT → POLL_RENDER, attempts `poll`, fails, backoff doubles a few times, then transitions POLL_RENDER → DISCOVER.
- DISCOVER runs mDNS, finds the real daemon, writes the correct host back to NVS, transitions DISCOVER → POLL_RENDER, polling resumes.

If this doesn't work, the mDNS fallback path in `perform_discover()` (Task 5) needs a fix.

- [ ] **Step 10: Note the flash size**

After all previous steps work, record the flash usage from `pio run -e esp32dev` output. It should be lower than the 87.1% baseline from project memory. If it's higher, something unintended grew.

---

## Self-review notes (resolved inline)

Verified during plan writing — no items to fix.

- **Spec coverage:** every requirement in the spec's "Firmware changes" section maps to a task (NVS schema → Task 1, USB provisioner module → Tasks 2–3, state machine → Task 4, boot logic → Task 5, mDNS fallback → Task 5 `perform_discover`, captive portal removal → Task 7, pairing UI removal → Tasks 6+7).
- **Placeholders:** no TBDs or "implement later" — every code block is complete.
- **Type consistency:** `ProvisioningCreds` field names match between Tasks 2, 3, and 5. NVS new method names (`server_host`, `server_port`, `bearer_token`, `save_server`, `save_bearer_token`, `has_server`, `has_bearer_token`) are used consistently in Tasks 1 and 5. `parse_provisioning_json` signature matches between Tasks 2 and 3.
