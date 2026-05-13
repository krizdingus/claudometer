# CYD Claude Monitor — Firmware MVP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the ESP32 firmware that boots a CYD, onboards onto WiFi, discovers and pairs with the running daemon, and renders six swipeable screens of Claude usage refreshed from `/v1/stats`.

**Architecture:** A single PlatformIO project under `firmware/` that compiles one Arduino-ESP32 binary targeting ESP32-2432S028C (capacitive) and ESP32-2432S028R (resistive). Runtime hardware probing picks the touch driver; the daemon stack uses built-in WiFi + HTTPClient + MDNS + ArduinoJson; the UI is LVGL 9.x driven by LovyanGFX on the ILI9341. Pure logic (JSON parsing, state machine) is unit-tested on the PlatformIO `native` env; hardware-touching code is manually verified on-device.

**Tech Stack:** PlatformIO + Arduino-ESP32 v3.x, LVGL 9.x, LovyanGFX, WiFiManager (tzapu), ArduinoJson v7, ESP32 built-in `MDNS` + `Preferences` + `HTTPClient`. Unity test framework via `pio test -e native`.

**Reference spec:** `docs/superpowers/specs/2026-05-13-cyd-claude-usage-monitor-design.md`
**Frozen API contract (do not deviate):** `daemon/internal/stats/types.go` — `Stats` struct with `schema=1`, fields `session`, `models_today`, `sonnet`, `chat`, `routines`, `budgets`. Pairing token is a 64-char lowercase hex string (32 random bytes); pairing code is a zero-padded 4-digit string; mDNS service is `_claudeusage._tcp.local.` on port `7842`.

**Branching:** Work on `feat/firmware-mvp` off `main`. Merge with `--no-ff` when done.

**Out of scope for this plan:**
- OTA updates (Phase 3).
- LVGL PC simulator + screenshot CI (Phase 3).
- Web UI / flasher site / brew formula (Phase 3).
- Claude.ai cookie support — the chat screen renders empty-state regardless.
- Multi-CYD pairing — the firmware stores exactly one daemon token.

---

## File Structure

```
firmware/
  platformio.ini                       # multi-env: native (unit tests), esp32dev (device)
  partitions.csv                       # custom: larger app + NVS partition
  src/
    main.cpp                           # arduino setup()/loop() entry
    app/
      state_machine.h                  # State enum, Event enum, Transition fn (pure)
      state_machine.cpp
      app_config.h                     # poll cadences, timeouts, colors, fonts
    hw/
      pins.h                           # ILI9341 + XPT2046 + FT6336 pin assignments
      display.h                        # LovyanGFX configured panel
      display.cpp
      touch.h                          # runtime FT6336 / XPT2046 detection
      touch.cpp
      nvs.h                            # Preferences wrapper (creds, token, hostname, calib)
      nvs.cpp
    net/
      wifi_onboarding.h                # WiFiManager bring-up + captive portal title
      wifi_onboarding.cpp
      mdns_discover.h                  # find _claudeusage._tcp + return host/port
      mdns_discover.cpp
      pairing_client.h                 # POST /v1/pair-init + /v1/pair-verify
      pairing_client.cpp
      stats_client.h                   # GET /v1/stats?screens=… with bearer token
      stats_client.cpp
      stats_parser.h                   # ArduinoJson → Stats C++ struct
      stats_parser.cpp
      stats_types.h                    # Mirror of daemon's stats.Stats (pure POCO)
    ui/
      theme.h                          # color + font constants, common LVGL styles
      theme.cpp
      chrome.h                         # status bar (top) + pip footer (bottom)
      chrome.cpp
      screen_session.h / .cpp          # 5h block ring
      screen_models.h / .cpp           # 3 horizontal model bars
      screen_sonnet.h / .cpp           # weekly budget bar
      screen_chat.h / .cpp             # daily messages (empty-state in MVP)
      screen_routines.h / .cpp         # routine list with status pills
      screen_budgets.h / .cpp          # 3 caps + plan tier + reset countdown
      tileview.h / .cpp                # carousel that owns the 6 screens
      provision_screen.h / .cpp        # "Join WiFi at ClaudeMonitor-XXXX" panel
      discover_screen.h / .cpp         # "Looking for daemon…" panel
      pair_screen.h / .cpp             # 4-digit code + tap-to-confirm
  test/
    native/
      test_stats_parser/test_main.cpp
      test_state_machine/test_main.cpp
    fixtures/
      stats_full.json                  # sample /v1/stats response
  scripts/
    e2e.sh                             # spin daemon, flash device, verify pairing
  README.md
```

Each `src/` subdirectory has one responsibility. `app/` is the pure state machine + global constants. `hw/` wraps the board. `net/` does I/O. `ui/` is all LVGL. `stats_parser` and `state_machine` are deliberately decoupled from Arduino/ESP-IDF symbols so they compile on the `native` env.

---

### Task 1: PlatformIO project scaffold

**Files:**
- Create: `firmware/platformio.ini`
- Create: `firmware/partitions.csv`
- Create: `firmware/src/main.cpp`
- Create: `firmware/.gitignore`
- Create: `firmware/README.md`

- [ ] **Step 1: Create the PlatformIO config**

```ini
; firmware/platformio.ini
[platformio]
default_envs = esp32dev

[env]
build_flags =
  -DCORE_DEBUG_LEVEL=3
  -std=gnu++17
build_unflags = -std=gnu++11

[env:esp32dev]
platform = espressif32 @ ^6.7.0
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 921600
board_build.partitions = partitions.csv
board_build.f_flash = 80000000L
build_flags =
  ${env.build_flags}
  -DBOARD_HAS_PSRAM=0
  -DLV_CONF_INCLUDE_SIMPLE
  -I src
lib_deps =
  lovyan03/LovyanGFX@^1.1.16
  lvgl/lvgl@^9.1.0
  bblanchon/ArduinoJson@^7.0.4
  tzapu/WiFiManager@^2.0.17

[env:native]
platform = native
test_framework = unity
build_flags =
  ${env.build_flags}
  -DUNIT_TEST
  -I src
  -I test/native
lib_deps =
  bblanchon/ArduinoJson@^7.0.4
```

- [ ] **Step 2: Create a custom partition table**

The default `default.csv` reserves only ~1.3 MB for the app; LVGL + LovyanGFX + WiFiManager push us close. Allocate 1.9 MB app, 64 KB NVS, no SPIFFS.

```csv
; firmware/partitions.csv
# Name,   Type, SubType, Offset,   Size,    Flags
nvs,      data, nvs,     0x9000,   0x10000,
otadata,  data, ota,     0x19000,  0x2000,
app0,     app,  ota_0,   0x20000,  0x1E0000,
app1,     app,  ota_1,   0x200000, 0x1E0000,
```

- [ ] **Step 3: Create the minimal main.cpp**

```cpp
// firmware/src/main.cpp
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("cydmonitor firmware booting");
}

void loop() {
  delay(1000);
}
```

- [ ] **Step 4: Create .gitignore**

```
# firmware/.gitignore
.pio/
.vscode/
.cache/
```

- [ ] **Step 5: Create README**

```markdown
# CYD Claude Monitor — Firmware

ESP32 firmware for the Cheap Yellow Display variant of the Claude Usage Monitor.

## Build

```
pio run -e esp32dev
```

## Upload to attached device

```
pio run -e esp32dev -t upload && pio device monitor
```

## Unit tests (host)

```
pio test -e native
```

## Hardware targets

- ESP32-2432S028C (capacitive, FT6336 touch) — primary
- ESP32-2432S028R (resistive, XPT2046 touch) — supported
```

- [ ] **Step 6: Verify the project compiles**

Run: `cd firmware && pio run -e esp32dev`
Expected: build succeeds, produces `.pio/build/esp32dev/firmware.bin`.

- [ ] **Step 7: Commit**

```bash
git checkout -b feat/firmware-mvp
git add firmware/
git commit -m "scaffold firmware PlatformIO project"
```

---

### Task 2: Stats types + JSON parser (native TDD)

**Files:**
- Create: `firmware/src/net/stats_types.h`
- Create: `firmware/src/net/stats_parser.h`
- Create: `firmware/src/net/stats_parser.cpp`
- Create: `firmware/test/native/test_stats_parser/test_main.cpp`
- Create: `firmware/test/fixtures/stats_full.json`

- [ ] **Step 1: Create the JSON fixture**

```json
{
  "schema": 1,
  "generated_at": "2026-05-13T14:23:00-07:00",
  "session": {
    "pct_used": 67,
    "minutes_remaining": 98,
    "resets_at": "16:01",
    "models": [
      {"model": "claude-sonnet-4-6", "tokens": 412000},
      {"model": "claude-opus-4-7", "tokens": 88000}
    ]
  },
  "models_today": {
    "total_tokens": 612000,
    "by_model": [
      {"model": "claude-opus-4-7", "tokens": 102000},
      {"model": "claude-sonnet-4-6", "tokens": 480000},
      {"model": "claude-haiku-4-5", "tokens": 30000}
    ],
    "est_cost_usd": 7.41
  },
  "sonnet": {
    "weekly_pct": 58,
    "used": 1160000,
    "cap": 2000000,
    "pace": "on_track"
  },
  "chat": {
    "messages_today": 42,
    "daily_cap": 200,
    "resets_at": "00:00"
  },
  "routines": [
    {"name": "babysit-prs", "status": "ok", "last_run": "12:00", "next_run": "17:00"},
    {"name": "morning-digest", "status": "slow", "last_run": "09:01", "next_run": "—"}
  ],
  "budgets": {
    "code_all": 71,
    "code_opus": 89,
    "chat": 22,
    "plan": "max-20x",
    "resets_in": "3d19h"
  }
}
```

Save the JSON above to `firmware/test/fixtures/stats_full.json` exactly as shown.

- [ ] **Step 2: Define the C++ types**

```cpp
// firmware/src/net/stats_types.h
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cyd {

struct ModelTokenRow {
  std::string model;
  int tokens = 0;
};

struct SessionStat {
  int pct_used = 0;
  int minutes_remaining = 0;
  std::string resets_at;
  std::vector<ModelTokenRow> models;
};

struct ModelsTodayStat {
  int total_tokens = 0;
  std::vector<ModelTokenRow> by_model;
  double est_cost_usd = 0.0;
};

struct SonnetWeeklyStat {
  int weekly_pct = 0;
  int used = 0;
  int cap = 0;
  std::string pace; // "behind" | "on_track" | "ahead"
};

struct ChatStat {
  int messages_today = 0;
  int daily_cap = 0;
  std::string resets_at;
};

struct RoutineStat {
  std::string name;
  std::string status;
  std::string last_run;
  std::string next_run;
};

struct BudgetsStat {
  int code_all = 0;
  int code_opus = 0;
  int chat = 0;
  std::string plan;
  std::string resets_in;
};

struct Stats {
  int schema = 0;
  std::string generated_at;
  SessionStat session;
  ModelsTodayStat models_today;
  SonnetWeeklyStat sonnet;
  ChatStat chat;
  std::vector<RoutineStat> routines;
  BudgetsStat budgets;
  bool stale = false; // set true when daemon last-known data is being shown
};

} // namespace cyd
```

- [ ] **Step 3: Write failing parser tests**

```cpp
// firmware/test/native/test_stats_parser/test_main.cpp
#include <unity.h>
#include <fstream>
#include <sstream>
#include <string>

#include "net/stats_parser.h"
#include "net/stats_types.h"

static std::string read_fixture() {
  std::ifstream f("test/fixtures/stats_full.json");
  TEST_ASSERT_TRUE_MESSAGE(f.good(), "fixture not found");
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

void test_parse_full_payload(void) {
  std::string body = read_fixture();
  cyd::Stats out;
  bool ok = cyd::parse_stats(body.c_str(), body.size(), out);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_INT(1, out.schema);
  TEST_ASSERT_EQUAL_INT(67, out.session.pct_used);
  TEST_ASSERT_EQUAL_INT(98, out.session.minutes_remaining);
  TEST_ASSERT_EQUAL_STRING("16:01", out.session.resets_at.c_str());
  TEST_ASSERT_EQUAL_size_t(2, out.session.models.size());
  TEST_ASSERT_EQUAL_INT(412000, out.session.models[0].tokens);

  TEST_ASSERT_EQUAL_INT(612000, out.models_today.total_tokens);
  TEST_ASSERT_EQUAL_size_t(3, out.models_today.by_model.size());
  TEST_ASSERT_FLOAT_WITHIN(0.001, 7.41, out.models_today.est_cost_usd);

  TEST_ASSERT_EQUAL_INT(58, out.sonnet.weekly_pct);
  TEST_ASSERT_EQUAL_STRING("on_track", out.sonnet.pace.c_str());

  TEST_ASSERT_EQUAL_INT(42, out.chat.messages_today);
  TEST_ASSERT_EQUAL_INT(200, out.chat.daily_cap);

  TEST_ASSERT_EQUAL_size_t(2, out.routines.size());
  TEST_ASSERT_EQUAL_STRING("babysit-prs", out.routines[0].name.c_str());
  TEST_ASSERT_EQUAL_STRING("slow", out.routines[1].status.c_str());

  TEST_ASSERT_EQUAL_INT(71, out.budgets.code_all);
  TEST_ASSERT_EQUAL_INT(89, out.budgets.code_opus);
  TEST_ASSERT_EQUAL_STRING("max-20x", out.budgets.plan.c_str());
  TEST_ASSERT_EQUAL_STRING("3d19h", out.budgets.resets_in.c_str());
}

void test_parse_rejects_garbage(void) {
  cyd::Stats out;
  const char *junk = "not json at all";
  TEST_ASSERT_FALSE(cyd::parse_stats(junk, 16, out));
}

void test_parse_handles_missing_optional_fields(void) {
  // Minimal payload: only schema + empty containers. Parser must accept it
  // (the daemon may someday omit fields with no data).
  std::string body = R"({"schema":1,"generated_at":"2026-05-13T00:00:00Z",
    "session":{"pct_used":0,"minutes_remaining":0,"resets_at":"","models":[]},
    "models_today":{"total_tokens":0,"by_model":[],"est_cost_usd":0},
    "sonnet":{"weekly_pct":0,"used":0,"cap":0,"pace":""},
    "chat":{"messages_today":0,"daily_cap":0,"resets_at":""},
    "routines":[],
    "budgets":{"code_all":0,"code_opus":0,"chat":0,"plan":"","resets_in":""}})";
  cyd::Stats out;
  TEST_ASSERT_TRUE(cyd::parse_stats(body.c_str(), body.size(), out));
  TEST_ASSERT_EQUAL_INT(1, out.schema);
  TEST_ASSERT_TRUE(out.routines.empty());
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_parse_full_payload);
  RUN_TEST(test_parse_rejects_garbage);
  RUN_TEST(test_parse_handles_missing_optional_fields);
  return UNITY_END();
}
```

- [ ] **Step 4: Run native tests to confirm they fail**

Run: `cd firmware && pio test -e native -f test_stats_parser`
Expected: link error — `parse_stats` undefined.

- [ ] **Step 5: Declare the parser header**

```cpp
// firmware/src/net/stats_parser.h
#pragma once

#include <cstddef>

#include "net/stats_types.h"

namespace cyd {

// parse_stats fills `out` from a UTF-8 JSON buffer. Returns false on malformed
// input or schema mismatch (schema != 1). Missing optional fields default to
// zero / empty string rather than failing.
bool parse_stats(const char *data, size_t len, Stats &out);

} // namespace cyd
```

- [ ] **Step 6: Implement the parser**

```cpp
// firmware/src/net/stats_parser.cpp
#include "net/stats_parser.h"

#include <ArduinoJson.h>

namespace cyd {

static ModelTokenRow read_row(JsonObjectConst obj) {
  ModelTokenRow r;
  r.model = obj["model"].as<const char *>() ? obj["model"].as<const char *>() : "";
  r.tokens = obj["tokens"] | 0;
  return r;
}

bool parse_stats(const char *data, size_t len, Stats &out) {
  // 8 KB doc per the spec. Payload is typically ~2 KB but routines may grow.
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) return false;

  out = Stats{};
  out.schema = doc["schema"] | 0;
  if (out.schema != 1) return false;
  out.generated_at = doc["generated_at"] | "";

  JsonObjectConst s = doc["session"].as<JsonObjectConst>();
  out.session.pct_used = s["pct_used"] | 0;
  out.session.minutes_remaining = s["minutes_remaining"] | 0;
  out.session.resets_at = s["resets_at"] | "";
  for (JsonObjectConst row : s["models"].as<JsonArrayConst>()) {
    out.session.models.push_back(read_row(row));
  }

  JsonObjectConst mt = doc["models_today"].as<JsonObjectConst>();
  out.models_today.total_tokens = mt["total_tokens"] | 0;
  out.models_today.est_cost_usd = mt["est_cost_usd"] | 0.0;
  for (JsonObjectConst row : mt["by_model"].as<JsonArrayConst>()) {
    out.models_today.by_model.push_back(read_row(row));
  }

  JsonObjectConst sn = doc["sonnet"].as<JsonObjectConst>();
  out.sonnet.weekly_pct = sn["weekly_pct"] | 0;
  out.sonnet.used = sn["used"] | 0;
  out.sonnet.cap = sn["cap"] | 0;
  out.sonnet.pace = sn["pace"] | "";

  JsonObjectConst ch = doc["chat"].as<JsonObjectConst>();
  out.chat.messages_today = ch["messages_today"] | 0;
  out.chat.daily_cap = ch["daily_cap"] | 0;
  out.chat.resets_at = ch["resets_at"] | "";

  for (JsonObjectConst r : doc["routines"].as<JsonArrayConst>()) {
    RoutineStat rs;
    rs.name = r["name"] | "";
    rs.status = r["status"] | "";
    rs.last_run = r["last_run"] | "";
    rs.next_run = r["next_run"] | "";
    out.routines.push_back(rs);
  }

  JsonObjectConst b = doc["budgets"].as<JsonObjectConst>();
  out.budgets.code_all = b["code_all"] | 0;
  out.budgets.code_opus = b["code_opus"] | 0;
  out.budgets.chat = b["chat"] | 0;
  out.budgets.plan = b["plan"] | "";
  out.budgets.resets_in = b["resets_in"] | "";

  return true;
}

} // namespace cyd
```

- [ ] **Step 7: Run tests and verify pass**

Run: `cd firmware && pio test -e native -f test_stats_parser`
Expected: 3 tests pass.

- [ ] **Step 8: Commit**

```bash
git add firmware/src/net firmware/test
git commit -m "add stats JSON parser with native unit tests"
```

---

### Task 3: State machine (native TDD)

**Files:**
- Create: `firmware/src/app/state_machine.h`
- Create: `firmware/src/app/state_machine.cpp`
- Create: `firmware/test/native/test_state_machine/test_main.cpp`

- [ ] **Step 1: Write the failing transition tests**

```cpp
// firmware/test/native/test_state_machine/test_main.cpp
#include <unity.h>

#include "app/state_machine.h"

using namespace cyd;

void test_boot_with_no_creds_goes_to_provision(void) {
  Context ctx{};
  ctx.have_wifi_creds = false;
  ctx.have_token = false;
  TEST_ASSERT_EQUAL(State::PROVISION, next_state(State::BOOT, Event::TICK, ctx));
}

void test_boot_with_creds_but_no_token_goes_to_discover(void) {
  Context ctx{};
  ctx.have_wifi_creds = true;
  ctx.have_token = false;
  TEST_ASSERT_EQUAL(State::DISCOVER, next_state(State::BOOT, Event::TICK, ctx));
}

void test_boot_with_creds_and_token_goes_to_poll(void) {
  Context ctx{};
  ctx.have_wifi_creds = true;
  ctx.have_token = true;
  TEST_ASSERT_EQUAL(State::POLL_RENDER, next_state(State::BOOT, Event::TICK, ctx));
}

void test_wifi_ok_advances_from_provision_to_discover(void) {
  Context ctx{};
  TEST_ASSERT_EQUAL(State::DISCOVER, next_state(State::PROVISION, Event::WIFI_OK, ctx));
}

void test_daemon_found_advances_from_discover_to_pair(void) {
  Context ctx{};
  ctx.have_token = false;
  TEST_ASSERT_EQUAL(State::PAIR, next_state(State::DISCOVER, Event::DAEMON_FOUND, ctx));
}

void test_daemon_found_with_token_jumps_to_poll(void) {
  Context ctx{};
  ctx.have_token = true;
  TEST_ASSERT_EQUAL(State::POLL_RENDER, next_state(State::DISCOVER, Event::DAEMON_FOUND, ctx));
}

void test_pair_confirmed_advances_to_poll(void) {
  Context ctx{};
  TEST_ASSERT_EQUAL(State::POLL_RENDER, next_state(State::PAIR, Event::PAIR_CONFIRMED, ctx));
}

void test_daemon_unreachable_marks_stale(void) {
  Context ctx{};
  TEST_ASSERT_EQUAL(State::POLL_RENDER, next_state(State::POLL_RENDER, Event::DAEMON_UNREACHABLE, ctx));
}

void test_long_press_resets_to_provision(void) {
  Context ctx{};
  TEST_ASSERT_EQUAL(State::PROVISION, next_state(State::POLL_RENDER, Event::FACTORY_RESET, ctx));
  TEST_ASSERT_EQUAL(State::PROVISION, next_state(State::PAIR, Event::FACTORY_RESET, ctx));
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_boot_with_no_creds_goes_to_provision);
  RUN_TEST(test_boot_with_creds_but_no_token_goes_to_discover);
  RUN_TEST(test_boot_with_creds_and_token_goes_to_poll);
  RUN_TEST(test_wifi_ok_advances_from_provision_to_discover);
  RUN_TEST(test_daemon_found_advances_from_discover_to_pair);
  RUN_TEST(test_daemon_found_with_token_jumps_to_poll);
  RUN_TEST(test_pair_confirmed_advances_to_poll);
  RUN_TEST(test_daemon_unreachable_marks_stale);
  RUN_TEST(test_long_press_resets_to_provision);
  return UNITY_END();
}
```

- [ ] **Step 2: Run tests to confirm failure**

Run: `cd firmware && pio test -e native -f test_state_machine`
Expected: link error — `next_state`, `State`, `Event`, `Context` undefined.

- [ ] **Step 3: Declare the state machine**

```cpp
// firmware/src/app/state_machine.h
#pragma once

namespace cyd {

enum class State {
  BOOT,
  PROVISION,    // AP mode, captive portal awaiting WiFi creds
  DISCOVER,     // WiFi up, scanning mDNS for daemon
  PAIR,         // showing 4-digit code, awaiting user confirmation
  POLL_RENDER,  // steady state: poll /v1/stats, draw screens
};

enum class Event {
  TICK,
  WIFI_OK,
  WIFI_FAIL,
  DAEMON_FOUND,
  DAEMON_NOT_FOUND,
  PAIR_CONFIRMED,
  PAIR_FAILED,
  DAEMON_UNREACHABLE,
  DAEMON_RECOVERED,
  FACTORY_RESET,
};

struct Context {
  bool have_wifi_creds = false;
  bool have_token = false;
};

// next_state is a pure function: given current state, an event, and the
// persistence context, return the state to transition to. Hardware effects
// (showing screens, opening sockets) are the caller's responsibility.
State next_state(State current, Event event, const Context &ctx);

} // namespace cyd
```

- [ ] **Step 4: Implement the state machine**

```cpp
// firmware/src/app/state_machine.cpp
#include "app/state_machine.h"

namespace cyd {

State next_state(State current, Event event, const Context &ctx) {
  if (event == Event::FACTORY_RESET) return State::PROVISION;

  switch (current) {
    case State::BOOT:
      if (!ctx.have_wifi_creds) return State::PROVISION;
      if (!ctx.have_token) return State::DISCOVER;
      return State::POLL_RENDER;

    case State::PROVISION:
      if (event == Event::WIFI_OK) return State::DISCOVER;
      return State::PROVISION;

    case State::DISCOVER:
      if (event == Event::DAEMON_FOUND) {
        return ctx.have_token ? State::POLL_RENDER : State::PAIR;
      }
      if (event == Event::WIFI_FAIL) return State::PROVISION;
      return State::DISCOVER;

    case State::PAIR:
      if (event == Event::PAIR_CONFIRMED) return State::POLL_RENDER;
      if (event == Event::PAIR_FAILED) return State::DISCOVER;
      return State::PAIR;

    case State::POLL_RENDER:
      // DAEMON_UNREACHABLE keeps us here; the caller flips the stale flag.
      return State::POLL_RENDER;
  }
  return current;
}

} // namespace cyd
```

- [ ] **Step 5: Run tests and verify pass**

Run: `cd firmware && pio test -e native -f test_state_machine`
Expected: 9 tests pass.

- [ ] **Step 6: Commit**

```bash
git add firmware/src/app firmware/test/native/test_state_machine
git commit -m "add app state machine with transition tests"
```

---

### Task 4: NVS persistence wrapper

**Files:**
- Create: `firmware/src/hw/nvs.h`
- Create: `firmware/src/hw/nvs.cpp`

NVS-backed storage is ESP32-only; we do not unit-test it on native. Instead we keep the surface tiny and verify on-device in Task 14.

- [ ] **Step 1: Declare the wrapper**

```cpp
// firmware/src/hw/nvs.h
#pragma once

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

  // Daemon pairing token (64-char lowercase hex).
  bool has_token() const;
  std::string token() const;
  void save_token(const std::string &token);

  // Last-known daemon host:port (e.g., "krizzo-mbp.local:7842").
  std::string daemon_host() const;
  void save_daemon_host(const std::string &host);

  // Touch calibration (resistive only). Returns true if cal values were stored.
  bool has_touch_cal() const;
  void load_touch_cal(uint16_t cal[8]) const;
  void save_touch_cal(const uint16_t cal[8]);

  // Wipe all keys, return to factory state.
  void factory_reset();
};

} // namespace cyd
```

- [ ] **Step 2: Implement the wrapper**

```cpp
// firmware/src/hw/nvs.cpp
#include "hw/nvs.h"

#include <Preferences.h>

namespace cyd {

static const char *kNs = "cydmon";
static const char *kSsid = "wifi_ssid";
static const char *kPsk  = "wifi_psk";
static const char *kTok  = "tok";
static const char *kHost = "host";
static const char *kCal  = "tcal";

void Nvs::begin() {
  Preferences p;
  p.begin(kNs, false);
  p.end();
}

bool Nvs::has_wifi_creds() const {
  Preferences p; p.begin(kNs, true);
  bool ok = p.isKey(kSsid) && p.isKey(kPsk);
  p.end();
  return ok;
}

std::string Nvs::wifi_ssid() const {
  Preferences p; p.begin(kNs, true);
  String s = p.getString(kSsid, "");
  p.end();
  return std::string(s.c_str());
}

std::string Nvs::wifi_psk() const {
  Preferences p; p.begin(kNs, true);
  String s = p.getString(kPsk, "");
  p.end();
  return std::string(s.c_str());
}

void Nvs::save_wifi(const std::string &ssid, const std::string &psk) {
  Preferences p; p.begin(kNs, false);
  p.putString(kSsid, ssid.c_str());
  p.putString(kPsk, psk.c_str());
  p.end();
}

bool Nvs::has_token() const {
  Preferences p; p.begin(kNs, true);
  bool ok = p.isKey(kTok);
  p.end();
  return ok;
}

std::string Nvs::token() const {
  Preferences p; p.begin(kNs, true);
  String s = p.getString(kTok, "");
  p.end();
  return std::string(s.c_str());
}

void Nvs::save_token(const std::string &t) {
  Preferences p; p.begin(kNs, false);
  p.putString(kTok, t.c_str());
  p.end();
}

std::string Nvs::daemon_host() const {
  Preferences p; p.begin(kNs, true);
  String s = p.getString(kHost, "");
  p.end();
  return std::string(s.c_str());
}

void Nvs::save_daemon_host(const std::string &h) {
  Preferences p; p.begin(kNs, false);
  p.putString(kHost, h.c_str());
  p.end();
}

bool Nvs::has_touch_cal() const {
  Preferences p; p.begin(kNs, true);
  bool ok = p.isKey(kCal);
  p.end();
  return ok;
}

void Nvs::load_touch_cal(uint16_t cal[8]) const {
  Preferences p; p.begin(kNs, true);
  p.getBytes(kCal, cal, sizeof(uint16_t) * 8);
  p.end();
}

void Nvs::save_touch_cal(const uint16_t cal[8]) {
  Preferences p; p.begin(kNs, false);
  p.putBytes(kCal, cal, sizeof(uint16_t) * 8);
  p.end();
}

void Nvs::factory_reset() {
  Preferences p; p.begin(kNs, false);
  p.clear();
  p.end();
}

} // namespace cyd
```

- [ ] **Step 3: Build to confirm it compiles**

Run: `cd firmware && pio run -e esp32dev`
Expected: build succeeds.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/hw/nvs.h firmware/src/hw/nvs.cpp
git commit -m "add NVS wrapper for wifi, token, host, touch calibration"
```

---

### Task 5: Pin map + display driver bring-up

**Files:**
- Create: `firmware/src/hw/pins.h`
- Create: `firmware/src/hw/display.h`
- Create: `firmware/src/hw/display.cpp`
- Modify: `firmware/src/main.cpp`

This is the first hardware-touching task — verify it on an actual CYD before continuing.

- [ ] **Step 1: Define the pin map**

```cpp
// firmware/src/hw/pins.h
#pragma once

// CYD ESP32-2432S028R/C standard pin assignments.
// The resistive (R) and capacitive (C) variants share the same display pins;
// only the touch interface differs.

namespace cyd::pins {

// ILI9341 TFT (HSPI)
constexpr int TFT_SCLK = 14;
constexpr int TFT_MOSI = 13;
constexpr int TFT_MISO = 12;
constexpr int TFT_CS   = 15;
constexpr int TFT_DC   = 2;
constexpr int TFT_RST  = -1;   // tied to EN on most boards
constexpr int TFT_BL   = 21;   // backlight (active high)

// XPT2046 resistive touch (VSPI) — R variant only
constexpr int XPT_SCLK = 25;
constexpr int XPT_MOSI = 32;
constexpr int XPT_MISO = 39;
constexpr int XPT_CS   = 33;
constexpr int XPT_IRQ  = 36;

// FT6336 capacitive touch (I²C) — C variant only.
// NOTE: these pins overlap with XPT2046 on R boards; runtime detection picks one.
constexpr int FT_SDA = 33;
constexpr int FT_SCL = 32;
constexpr uint8_t FT_ADDR = 0x38;

} // namespace cyd::pins
```

- [ ] **Step 2: Declare the display module**

```cpp
// firmware/src/hw/display.h
#pragma once

#include <LovyanGFX.hpp>

namespace cyd {

class CydDisplay : public lgfx::LGFX_Device {
 public:
  CydDisplay();

 private:
  lgfx::Panel_ILI9341 panel_;
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM light_;
};

// Singleton accessor used everywhere.
CydDisplay &display();

} // namespace cyd
```

- [ ] **Step 3: Implement the LovyanGFX configuration**

```cpp
// firmware/src/hw/display.cpp
#include "hw/display.h"

#include "hw/pins.h"

namespace cyd {

CydDisplay::CydDisplay() {
  {
    auto cfg = bus_.config();
    cfg.spi_host = HSPI_HOST;
    cfg.spi_mode = 0;
    cfg.freq_write = 40000000;
    cfg.freq_read = 16000000;
    cfg.spi_3wire = false;
    cfg.use_lock = true;
    cfg.dma_channel = SPI_DMA_CH_AUTO;
    cfg.pin_sclk = cyd::pins::TFT_SCLK;
    cfg.pin_mosi = cyd::pins::TFT_MOSI;
    cfg.pin_miso = cyd::pins::TFT_MISO;
    cfg.pin_dc   = cyd::pins::TFT_DC;
    bus_.config(cfg);
    panel_.setBus(&bus_);
  }
  {
    auto cfg = panel_.config();
    cfg.pin_cs   = cyd::pins::TFT_CS;
    cfg.pin_rst  = cyd::pins::TFT_RST;
    cfg.pin_busy = -1;
    cfg.panel_width  = 240;
    cfg.panel_height = 320;
    cfg.offset_rotation = 0;
    cfg.readable = false;
    cfg.invert = false;
    cfg.rgb_order = false;
    cfg.dlen_16bit = false;
    cfg.bus_shared = false;
    panel_.config(cfg);
  }
  {
    auto cfg = light_.config();
    cfg.pin_bl = cyd::pins::TFT_BL;
    cfg.invert = false;
    cfg.freq = 12000;
    cfg.pwm_channel = 7;
    light_.config(cfg);
    panel_.setLight(&light_);
  }
  setPanel(&panel_);
}

CydDisplay &display() {
  static CydDisplay d;
  return d;
}

} // namespace cyd
```

- [ ] **Step 4: Add a smoke screen to main.cpp**

```cpp
// firmware/src/main.cpp
#include <Arduino.h>

#include "hw/display.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("cydmonitor firmware booting");

  auto &lcd = cyd::display();
  lcd.init();
  lcd.setRotation(0);
  lcd.setBrightness(200);

  lcd.fillScreen(0x0000);                // black
  lcd.fillRect(0, 0, 240, 80, 0xF800);   // red top
  lcd.fillRect(0, 80, 240, 80, 0x07E0);  // green middle
  lcd.fillRect(0, 160, 240, 80, 0x001F); // blue
  lcd.fillRect(0, 240, 240, 80, 0xFFE0); // yellow bottom
  lcd.setTextColor(0xFFFF, 0x0000);
  lcd.setCursor(20, 300);
  lcd.printf("CYD display OK");
}

void loop() {
  delay(1000);
}
```

- [ ] **Step 5: Flash and verify on hardware**

Run:
```bash
cd firmware && pio run -e esp32dev -t upload && pio device monitor
```

Expected:
- Four colored bands top-to-bottom (red, green, blue, yellow).
- Serial prints "cydmonitor firmware booting".
- Backlight on, no flicker.

If colors look wrong (e.g., red and blue swapped), flip `cfg.rgb_order` in `display.cpp` and re-flash.

- [ ] **Step 6: Commit**

```bash
git add firmware/src/hw/pins.h firmware/src/hw/display.h firmware/src/hw/display.cpp firmware/src/main.cpp
git commit -m "bring up ILI9341 display via LovyanGFX"
```

---

### Task 6: Touch detection + driver

**Files:**
- Create: `firmware/src/hw/touch.h`
- Create: `firmware/src/hw/touch.cpp`
- Modify: `firmware/src/main.cpp`

- [ ] **Step 1: Declare the touch abstraction**

```cpp
// firmware/src/hw/touch.h
#pragma once

#include <cstdint>

namespace cyd {

enum class TouchKind { None, Capacitive, Resistive };

struct TouchEvent {
  bool pressed = false;
  int16_t x = -1;
  int16_t y = -1;
};

class Touch {
 public:
  // probe() does an I²C scan for FT6336; on success initialises capacitive
  // mode. On failure, initialises XPT2046 over VSPI in resistive mode. The
  // result is cached.
  TouchKind probe_and_init();

  TouchKind kind() const { return kind_; }

  // Non-blocking read; returns pressed=false when no touch is active.
  TouchEvent poll();

 private:
  TouchKind kind_ = TouchKind::None;
};

Touch &touch();

} // namespace cyd
```

- [ ] **Step 2: Implement probing + both drivers**

```cpp
// firmware/src/hw/touch.cpp
#include "hw/touch.h"

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include "hw/pins.h"

namespace cyd {

namespace {
SPIClass xpt_spi(VSPI);

bool ft6336_present() {
  Wire.begin(pins::FT_SDA, pins::FT_SCL, 400000);
  Wire.beginTransmission(pins::FT_ADDR);
  return Wire.endTransmission() == 0;
}

void init_xpt2046() {
  pinMode(pins::XPT_CS, OUTPUT);
  digitalWrite(pins::XPT_CS, HIGH);
  xpt_spi.begin(pins::XPT_SCLK, pins::XPT_MISO, pins::XPT_MOSI, pins::XPT_CS);
  xpt_spi.setFrequency(2000000);
}

TouchEvent read_ft6336() {
  TouchEvent ev;
  Wire.beginTransmission(pins::FT_ADDR);
  Wire.write(0x02);                  // num touches register
  if (Wire.endTransmission(false) != 0) return ev;
  Wire.requestFrom(pins::FT_ADDR, (uint8_t)5);
  if (Wire.available() < 5) return ev;
  uint8_t n = Wire.read();
  uint8_t xh = Wire.read();
  uint8_t xl = Wire.read();
  uint8_t yh = Wire.read();
  uint8_t yl = Wire.read();
  if (n == 0) return ev;
  ev.pressed = true;
  ev.x = ((xh & 0x0F) << 8) | xl;
  ev.y = ((yh & 0x0F) << 8) | yl;
  return ev;
}

TouchEvent read_xpt2046() {
  TouchEvent ev;
  if (digitalRead(pins::XPT_IRQ) == HIGH) return ev;  // IRQ low = pressed
  digitalWrite(pins::XPT_CS, LOW);
  xpt_spi.transfer(0xD0);                              // X channel
  uint16_t x = (xpt_spi.transfer(0) << 8 | xpt_spi.transfer(0)) >> 3;
  xpt_spi.transfer(0x90);                              // Y channel
  uint16_t y = (xpt_spi.transfer(0) << 8 | xpt_spi.transfer(0)) >> 3;
  digitalWrite(pins::XPT_CS, HIGH);
  if (x == 0 && y == 0) return ev;
  ev.pressed = true;
  // Map raw 12-bit to 240x320. Calibration is loaded from NVS in production;
  // this fallback assumes a roughly linear mapping.
  ev.x = map(x, 200, 3900, 0, 240);
  ev.y = map(y, 240, 3800, 0, 320);
  return ev;
}
}  // namespace

TouchKind Touch::probe_and_init() {
  if (ft6336_present()) {
    kind_ = TouchKind::Capacitive;
    Serial.println("touch: FT6336 (capacitive) detected");
  } else {
    init_xpt2046();
    kind_ = TouchKind::Resistive;
    Serial.println("touch: XPT2046 (resistive) fallback");
  }
  return kind_;
}

TouchEvent Touch::poll() {
  if (kind_ == TouchKind::Capacitive) return read_ft6336();
  if (kind_ == TouchKind::Resistive) return read_xpt2046();
  return {};
}

Touch &touch() {
  static Touch t;
  return t;
}

} // namespace cyd
```

- [ ] **Step 3: Wire touch into main.cpp**

Replace `main.cpp` setup() with:

```cpp
#include <Arduino.h>

#include "hw/display.h"
#include "hw/touch.h"

void setup() {
  Serial.begin(115200);
  delay(200);

  auto &lcd = cyd::display();
  lcd.init();
  lcd.setRotation(0);
  lcd.setBrightness(200);
  lcd.fillScreen(0x0000);

  auto kind = cyd::touch().probe_and_init();
  lcd.setTextColor(0xFFFF, 0x0000);
  lcd.setCursor(10, 10);
  lcd.printf("touch: %s",
             kind == cyd::TouchKind::Capacitive ? "capacitive"
             : kind == cyd::TouchKind::Resistive ? "resistive" : "none");
}

void loop() {
  auto ev = cyd::touch().poll();
  if (ev.pressed) {
    Serial.printf("touch @ (%d, %d)\n", ev.x, ev.y);
    cyd::display().fillCircle(ev.x, ev.y, 4, 0xFFFF);
  }
  delay(20);
}
```

- [ ] **Step 4: Flash and verify on both board variants if available**

Run: `pio run -e esp32dev -t upload && pio device monitor`

Expected:
- Top-left text reads either `touch: capacitive` or `touch: resistive`.
- Each finger tap draws a small white dot at approximately the right pixel; serial logs `touch @ (x, y)`.

If you only have one variant, verify just that one; the runtime branch for the other is exercised on a host with that board.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/hw/touch.h firmware/src/hw/touch.cpp firmware/src/main.cpp
git commit -m "add runtime FT6336/XPT2046 touch detection"
```

---

### Task 7: LVGL bring-up

**Files:**
- Create: `firmware/src/ui/lvgl_glue.h`
- Create: `firmware/src/ui/lvgl_glue.cpp`
- Create: `firmware/lv_conf.h`
- Modify: `firmware/platformio.ini`
- Modify: `firmware/src/main.cpp`

- [ ] **Step 1: Add the LVGL config**

`lv_conf.h` is a stripped-down LVGL 9.x config tuned for our buffer size and color depth.

```c
// firmware/lv_conf.h
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

#define LV_USE_OS LV_OS_NONE
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_MEM_SIZE (48U * 1024U)

#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1

#define LV_USE_ARC      1
#define LV_USE_BAR      1
#define LV_USE_BTN      1
#define LV_USE_LABEL    1
#define LV_USE_LINE     1
#define LV_USE_TILEVIEW 1
#define LV_USE_IMG      1

#endif // LV_CONF_H
```

- [ ] **Step 2: Tell PlatformIO about lv_conf.h**

Add the include flag to `[env:esp32dev]` in `platformio.ini`:

```ini
build_flags =
  ${env.build_flags}
  -DBOARD_HAS_PSRAM=0
  -DLV_CONF_INCLUDE_SIMPLE
  -DLV_CONF_PATH="${PROJECT_DIR}/lv_conf.h"
  -I src
```

- [ ] **Step 3: Declare the LVGL glue**

```cpp
// firmware/src/ui/lvgl_glue.h
#pragma once

namespace cyd {

// Initialise LVGL, register display + input devices, allocate draw buffers.
// Must be called once after display() and touch() are initialised.
void lvgl_init();

// Drive LVGL's timers + flush queue. Call from loop().
void lvgl_tick();

} // namespace cyd
```

- [ ] **Step 4: Implement the glue**

```cpp
// firmware/src/ui/lvgl_glue.cpp
#include "ui/lvgl_glue.h"

#include <Arduino.h>
#include <lvgl.h>

#include "hw/display.h"
#include "hw/touch.h"

namespace cyd {

namespace {

constexpr uint32_t kScreenW = 240;
constexpr uint32_t kScreenH = 320;
constexpr uint32_t kBufRows = 40;

// Two partial draw buffers (double-buffered). 240 * 40 * 2 bytes = 19200 each.
static lv_color_t buf_a[kScreenW * kBufRows];
static lv_color_t buf_b[kScreenW * kBufRows];

void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  auto w = area->x2 - area->x1 + 1;
  auto h = area->y2 - area->y1 + 1;
  display().pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t *)px_map);
  lv_display_flush_ready(disp);
}

void touch_read_cb(lv_indev_t *, lv_indev_data_t *data) {
  auto ev = touch().poll();
  data->state = ev.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  if (ev.pressed) {
    data->point.x = ev.x;
    data->point.y = ev.y;
  }
}

} // namespace

void lvgl_init() {
  lv_init();

  auto *disp = lv_display_create(kScreenW, kScreenH);
  lv_display_set_buffers(disp, buf_a, buf_b, sizeof(buf_a),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, flush_cb);

  auto *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_read_cb);
}

void lvgl_tick() {
  lv_timer_handler();
}

} // namespace cyd
```

- [ ] **Step 5: Add a hello-world LVGL screen in main.cpp**

```cpp
#include <Arduino.h>
#include <lvgl.h>

#include "hw/display.h"
#include "hw/touch.h"
#include "ui/lvgl_glue.h"

void setup() {
  Serial.begin(115200);
  cyd::display().init();
  cyd::display().setRotation(0);
  cyd::display().setBrightness(200);
  cyd::display().fillScreen(0x0000);
  cyd::touch().probe_and_init();
  cyd::lvgl_init();

  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0E0E10), 0);
  lv_obj_t *label = lv_label_create(scr);
  lv_label_set_text(label, "LVGL OK\nclaude monitor");
  lv_obj_set_style_text_color(label, lv_color_hex(0xD97757), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

void loop() {
  cyd::lvgl_tick();
  delay(5);
}
```

- [ ] **Step 6: Flash and verify**

Run: `pio run -e esp32dev -t upload && pio device monitor`

Expected:
- Dark `#0E0E10` background.
- Two-line orange `LVGL OK / claude monitor` text centered.
- No LVGL warnings on serial.

- [ ] **Step 7: Commit**

```bash
git add firmware/lv_conf.h firmware/platformio.ini firmware/src/ui/lvgl_glue.h firmware/src/ui/lvgl_glue.cpp firmware/src/main.cpp
git commit -m "bring up LVGL with double-buffered partial rendering"
```

---

### Task 8: Theme module

**Files:**
- Create: `firmware/src/ui/theme.h`
- Create: `firmware/src/ui/theme.cpp`
- Create: `firmware/src/app/app_config.h`

- [ ] **Step 1: Define the app config constants**

```cpp
// firmware/src/app/app_config.h
#pragma once

#include <cstdint>

namespace cyd {

// Polling cadences (milliseconds).
constexpr uint32_t kActivePollMs = 5000;
constexpr uint32_t kIdlePollMs   = 30000;

// Network timeouts.
constexpr uint32_t kHttpTimeoutMs = 4000;
constexpr uint32_t kMdnsQueryMs   = 2500;

// Backoff schedule for daemon-unreachable: 1s, 2s, 4s, …, capped at 60s.
constexpr uint32_t kBackoffStartMs = 1000;
constexpr uint32_t kBackoffMaxMs   = 60000;

// Long-press duration before factory reset (ms).
constexpr uint32_t kLongPressMs = 5000;

// Screen index → bit. The CYD only fetches screens within ±1 of the current
// one, so this is also the request mask.
enum Screen : uint8_t {
  SCR_SESSION = 0,
  SCR_MODELS,
  SCR_SONNET,
  SCR_CHAT,
  SCR_ROUTINES,
  SCR_BUDGETS,
  SCR_COUNT,
};

constexpr int kStatusBarHeight = 18;
constexpr int kFooterHeight    = 18;

} // namespace cyd
```

- [ ] **Step 2: Declare the theme**

```cpp
// firmware/src/ui/theme.h
#pragma once

#include <lvgl.h>

namespace cyd::theme {

constexpr uint32_t bg          = 0x0E0E10;
constexpr uint32_t fg          = 0xEDEDED;
constexpr uint32_t fg_muted    = 0x8A8A92;
constexpr uint32_t accent      = 0xD97757;   // Claude orange
constexpr uint32_t blue        = 0x6F9EFF;
constexpr uint32_t red         = 0xE85C5C;
constexpr uint32_t yellow      = 0xF5D24A;   // bezel chrome only
constexpr uint32_t green       = 0x7BD389;

inline lv_color_t c(uint32_t hex) { return lv_color_hex(hex); }

// Returns blue normally; red when pct >= 85.
lv_color_t bar_color_for_pct(int pct);

// Returns the appropriate pill background color for routine status.
lv_color_t status_pill_for(const char *status);

// Common style applied to all tile screens (bg + padding).
void apply_screen_styles(lv_obj_t *scr);

} // namespace cyd::theme
```

- [ ] **Step 3: Implement**

```cpp
// firmware/src/ui/theme.cpp
#include "ui/theme.h"

#include <string.h>

namespace cyd::theme {

lv_color_t bar_color_for_pct(int pct) {
  return pct >= 85 ? c(red) : c(blue);
}

lv_color_t status_pill_for(const char *status) {
  if (!status) return c(fg_muted);
  if (strcmp(status, "ok") == 0)     return c(green);
  if (strcmp(status, "slow") == 0)   return c(yellow);
  if (strcmp(status, "fail") == 0)   return c(red);
  if (strcmp(status, "queued") == 0) return c(blue);
  return c(fg_muted);
}

void apply_screen_styles(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, c(bg), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(scr, 8, 0);
  lv_obj_set_style_text_color(scr, c(fg), 0);
  lv_obj_set_style_text_font(scr, &lv_font_montserrat_14, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
}

} // namespace cyd::theme
```

- [ ] **Step 4: Verify build**

Run: `cd firmware && pio run -e esp32dev`
Expected: build succeeds.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/ui/theme.h firmware/src/ui/theme.cpp firmware/src/app/app_config.h
git commit -m "add theme tokens and global UI constants"
```

---

### Task 9: Status bar + pip footer

**Files:**
- Create: `firmware/src/ui/chrome.h`
- Create: `firmware/src/ui/chrome.cpp`

- [ ] **Step 1: Declare chrome**

```cpp
// firmware/src/ui/chrome.h
#pragma once

#include <lvgl.h>

#include "app/app_config.h"

namespace cyd {

class Chrome {
 public:
  // Attach permanent status bar (top) and pip footer (bottom) to a parent.
  // The parent is typically the active screen or a layer above tileview.
  void attach(lv_obj_t *parent);

  // health: 0 = ok (green dot), 1 = stale (yellow), 2 = offline (red).
  void set_health(int health);
  void set_clock(const char *hhmm);
  void set_active_screen(int index);

 private:
  lv_obj_t *dot_ = nullptr;
  lv_obj_t *clock_ = nullptr;
  lv_obj_t *pips_[SCR_COUNT] = {nullptr};
};

} // namespace cyd
```

- [ ] **Step 2: Implement chrome**

```cpp
// firmware/src/ui/chrome.cpp
#include "ui/chrome.h"

#include "ui/theme.h"

namespace cyd {

void Chrome::attach(lv_obj_t *parent) {
  // Status bar
  auto *bar = lv_obj_create(parent);
  lv_obj_set_size(bar, 240, kStatusBarHeight);
  lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(bar, theme::c(theme::bg), 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_pad_all(bar, 2, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  dot_ = lv_obj_create(bar);
  lv_obj_set_size(dot_, 8, 8);
  lv_obj_align(dot_, LV_ALIGN_LEFT_MID, 4, 0);
  lv_obj_set_style_radius(dot_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(dot_, 0, 0);
  lv_obj_set_style_bg_color(dot_, theme::c(theme::green), 0);

  clock_ = lv_label_create(bar);
  lv_obj_align(clock_, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_set_style_text_color(clock_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(clock_, &lv_font_montserrat_12, 0);
  lv_label_set_text(clock_, "--:--");

  // Pip footer
  auto *foot = lv_obj_create(parent);
  lv_obj_set_size(foot, 240, kFooterHeight);
  lv_obj_align(foot, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(foot, theme::c(theme::bg), 0);
  lv_obj_set_style_border_width(foot, 0, 0);
  lv_obj_set_style_pad_all(foot, 2, 0);
  lv_obj_clear_flag(foot, LV_OBJ_FLAG_SCROLLABLE);

  constexpr int kPipSize = 6;
  constexpr int kPipGap  = 10;
  int total = SCR_COUNT * kPipSize + (SCR_COUNT - 1) * kPipGap;
  int x0 = (240 - total) / 2;
  for (int i = 0; i < SCR_COUNT; ++i) {
    pips_[i] = lv_obj_create(foot);
    lv_obj_set_size(pips_[i], kPipSize, kPipSize);
    lv_obj_set_pos(pips_[i], x0 + i * (kPipSize + kPipGap), 4);
    lv_obj_set_style_radius(pips_[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(pips_[i], 0, 0);
    lv_obj_set_style_bg_color(pips_[i], theme::c(theme::fg_muted), 0);
  }
  set_active_screen(0);
}

void Chrome::set_health(int health) {
  uint32_t color = theme::green;
  if (health == 1) color = theme::yellow;
  if (health == 2) color = theme::red;
  lv_obj_set_style_bg_color(dot_, theme::c(color), 0);
}

void Chrome::set_clock(const char *hhmm) {
  lv_label_set_text(clock_, hhmm ? hhmm : "--:--");
}

void Chrome::set_active_screen(int index) {
  for (int i = 0; i < SCR_COUNT; ++i) {
    bool on = (i == index);
    lv_obj_set_style_bg_color(pips_[i],
                              theme::c(on ? theme::accent : theme::fg_muted), 0);
  }
}

} // namespace cyd
```

- [ ] **Step 3: Smoke-test in main.cpp**

Replace the body of `setup()` after `lvgl_init()` with:

```cpp
auto *scr = lv_screen_active();
lv_obj_set_style_bg_color(scr, lv_color_hex(0x0E0E10), 0);

static cyd::Chrome chrome;
chrome.attach(scr);
chrome.set_health(0);
chrome.set_clock("14:23");
chrome.set_active_screen(2);
```

- [ ] **Step 4: Flash and verify**

Run: `pio run -e esp32dev -t upload`

Expected:
- Top 18 px: green dot at left, `14:23` at right.
- Bottom 18 px: six small dots centered, with the third one in orange and the rest grey.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/ui/chrome.h firmware/src/ui/chrome.cpp firmware/src/main.cpp
git commit -m "add persistent status bar and pip footer chrome"
```

---

### Task 10: Tileview carousel scaffold

**Files:**
- Create: `firmware/src/ui/tileview.h`
- Create: `firmware/src/ui/tileview.cpp`

- [ ] **Step 1: Declare the tileview**

```cpp
// firmware/src/ui/tileview.h
#pragma once

#include <lvgl.h>

#include "app/app_config.h"

namespace cyd {

// Owns one lv_tileview with 6 horizontal tiles in a vertical strip. Each
// concrete screen module (screen_session, …) receives its tile root and
// fills it in. The tileview itself is sized to (240, 320 - chrome).
class Tileview {
 public:
  void attach(lv_obj_t *parent);
  lv_obj_t *tile(Screen s);
  Screen active() const;
  void set_active(Screen s, lv_anim_enable_t anim = LV_ANIM_ON);

  // Returns the bitmask of screens within ±1 of the active one (the screens
  // the firmware will request from the daemon).
  uint8_t neighbor_mask() const;

  // Called on every active-screen change to keep chrome in sync.
  using ChangeCb = void (*)(Screen);
  void on_change(ChangeCb cb) { change_cb_ = cb; }

 private:
  lv_obj_t *tv_ = nullptr;
  lv_obj_t *tiles_[SCR_COUNT] = {nullptr};
  ChangeCb change_cb_ = nullptr;

  static void event_cb(lv_event_t *e);
};

} // namespace cyd
```

- [ ] **Step 2: Implement the tileview**

```cpp
// firmware/src/ui/tileview.cpp
#include "ui/tileview.h"

#include "ui/theme.h"

namespace cyd {

void Tileview::attach(lv_obj_t *parent) {
  tv_ = lv_tileview_create(parent);
  lv_obj_set_size(tv_, 240, 320 - kStatusBarHeight - kFooterHeight);
  lv_obj_align(tv_, LV_ALIGN_TOP_MID, 0, kStatusBarHeight);
  lv_obj_set_style_bg_color(tv_, theme::c(theme::bg), 0);
  lv_obj_set_style_border_width(tv_, 0, 0);
  lv_obj_set_scrollbar_mode(tv_, LV_SCROLLBAR_MODE_OFF);

  for (int i = 0; i < SCR_COUNT; ++i) {
    tiles_[i] = lv_tileview_add_tile(tv_, i, 0, LV_DIR_HOR);
    theme::apply_screen_styles(tiles_[i]);
  }
  lv_obj_set_user_data(tv_, this);
  lv_obj_add_event_cb(tv_, &Tileview::event_cb, LV_EVENT_VALUE_CHANGED, this);
}

lv_obj_t *Tileview::tile(Screen s) { return tiles_[s]; }

Screen Tileview::active() const {
  lv_obj_t *cur = lv_tileview_get_tile_active(tv_);
  for (int i = 0; i < SCR_COUNT; ++i) {
    if (tiles_[i] == cur) return static_cast<Screen>(i);
  }
  return SCR_SESSION;
}

void Tileview::set_active(Screen s, lv_anim_enable_t anim) {
  lv_tileview_set_tile(tv_, tiles_[s], anim);
}

uint8_t Tileview::neighbor_mask() const {
  int a = active();
  uint8_t mask = 1 << a;
  if (a > 0)              mask |= 1 << (a - 1);
  if (a < SCR_COUNT - 1)  mask |= 1 << (a + 1);
  return mask;
}

void Tileview::event_cb(lv_event_t *e) {
  auto *self = static_cast<Tileview *>(lv_event_get_user_data(e));
  if (self && self->change_cb_) self->change_cb_(self->active());
}

} // namespace cyd
```

- [ ] **Step 3: Wire up in main.cpp**

Add to `setup()` after `chrome.attach(scr)`:

```cpp
static cyd::Tileview tv;
tv.attach(scr);
tv.on_change([](cyd::Screen s) {
  // chrome lives in setup() scope; the real wiring goes in Task 17.
});

// Sanity placeholders so we can verify swiping works.
for (int i = 0; i < cyd::SCR_COUNT; ++i) {
  auto *t = tv.tile(static_cast<cyd::Screen>(i));
  auto *lbl = lv_label_create(t);
  lv_label_set_text_fmt(lbl, "screen %d", i);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0xD97757), 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
  lv_obj_center(lbl);
}
```

- [ ] **Step 4: Flash and verify swiping**

Run: `pio run -e esp32dev -t upload`

Expected:
- Initial screen reads `screen 0`.
- Swiping right→left advances through `screen 1` … `screen 5`.
- Status bar and pip footer remain visible.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/ui/tileview.h firmware/src/ui/tileview.cpp firmware/src/main.cpp
git commit -m "add tileview carousel with neighbor-mask helper"
```

---

### Task 11: Screen — Session

**Files:**
- Create: `firmware/src/ui/screen_session.h`
- Create: `firmware/src/ui/screen_session.cpp`

- [ ] **Step 1: Declare the screen**

```cpp
// firmware/src/ui/screen_session.h
#pragma once

#include <lvgl.h>

#include "net/stats_types.h"

namespace cyd {

class ScreenSession {
 public:
  void build(lv_obj_t *parent);
  void update(const Stats &s);

 private:
  lv_obj_t *arc_ = nullptr;
  lv_obj_t *pct_label_ = nullptr;
  lv_obj_t *resets_label_ = nullptr;
  lv_obj_t *model_a_ = nullptr;
  lv_obj_t *model_b_ = nullptr;
};

} // namespace cyd
```

- [ ] **Step 2: Implement the session ring**

```cpp
// firmware/src/ui/screen_session.cpp
#include "ui/screen_session.h"

#include <stdio.h>

#include "ui/theme.h"

namespace cyd {

void ScreenSession::build(lv_obj_t *parent) {
  // Section title
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Session");
  lv_obj_set_style_text_color(title, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 4);

  // 5h block ring
  arc_ = lv_arc_create(parent);
  lv_obj_set_size(arc_, 160, 160);
  lv_obj_align(arc_, LV_ALIGN_TOP_MID, 0, 22);
  lv_arc_set_rotation(arc_, 270);
  lv_arc_set_bg_angles(arc_, 0, 360);
  lv_arc_set_range(arc_, 0, 100);
  lv_obj_remove_style(arc_, NULL, LV_PART_KNOB);
  lv_obj_set_style_arc_width(arc_, 14, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc_, 14, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc_, theme::c(0x222226), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc_, theme::c(theme::accent), LV_PART_INDICATOR);
  lv_obj_clear_flag(arc_, LV_OBJ_FLAG_CLICKABLE);

  // Center: big % number
  pct_label_ = lv_label_create(arc_);
  lv_obj_set_style_text_color(pct_label_, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(pct_label_, &lv_font_montserrat_32, 0);
  lv_label_set_text(pct_label_, "--%");
  lv_obj_center(pct_label_);

  // Resets at label below the ring
  resets_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(resets_label_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(resets_label_, &lv_font_montserrat_14, 0);
  lv_obj_align(resets_label_, LV_ALIGN_TOP_MID, 0, 188);
  lv_label_set_text(resets_label_, "resets --:--");

  // Top 2 models
  model_a_ = lv_label_create(parent);
  model_b_ = lv_label_create(parent);
  for (auto *m : {model_a_, model_b_}) {
    lv_obj_set_style_text_color(m, theme::c(theme::fg), 0);
    lv_obj_set_style_text_font(m, &lv_font_montserrat_14, 0);
  }
  lv_obj_align(model_a_, LV_ALIGN_TOP_LEFT, 8, 218);
  lv_obj_align(model_b_, LV_ALIGN_TOP_LEFT, 8, 240);
  lv_label_set_text(model_a_, "");
  lv_label_set_text(model_b_, "");
}

static const char *short_name(const std::string &m) {
  if (m.find("opus") != std::string::npos)   return "Opus";
  if (m.find("sonnet") != std::string::npos) return "Sonnet";
  if (m.find("haiku") != std::string::npos)  return "Haiku";
  return m.c_str();
}

void ScreenSession::update(const Stats &s) {
  int pct = s.session.pct_used;
  lv_arc_set_value(arc_, pct);
  lv_obj_set_style_arc_color(arc_, theme::bar_color_for_pct(pct), LV_PART_INDICATOR);
  char buf[16];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  lv_label_set_text(pct_label_, buf);

  if (!s.session.resets_at.empty()) {
    char r[40];
    snprintf(r, sizeof(r), "resets %s · %dm left",
             s.session.resets_at.c_str(), s.session.minutes_remaining);
    lv_label_set_text(resets_label_, r);
  } else {
    lv_label_set_text(resets_label_, "no active session");
  }

  auto fmt_row = [](lv_obj_t *lbl, const ModelTokenRow *row) {
    if (!row) { lv_label_set_text(lbl, ""); return; }
    char r[40];
    snprintf(r, sizeof(r), "%s  %dk", short_name(row->model), row->tokens / 1000);
    lv_label_set_text(lbl, r);
  };
  fmt_row(model_a_, s.session.models.size() > 0 ? &s.session.models[0] : nullptr);
  fmt_row(model_b_, s.session.models.size() > 1 ? &s.session.models[1] : nullptr);
}
```

- [ ] **Step 3: Smoke-test with a stub Stats value**

In `main.cpp`, after building the tileview, drop in:

```cpp
static cyd::ScreenSession session;
session.build(tv.tile(cyd::SCR_SESSION));

cyd::Stats stub{};
stub.schema = 1;
stub.session.pct_used = 67;
stub.session.minutes_remaining = 98;
stub.session.resets_at = "16:01";
stub.session.models = {
  {"claude-sonnet-4-6", 412000},
  {"claude-opus-4-7", 88000},
};
session.update(stub);
```

- [ ] **Step 4: Flash and verify**

Run: `pio run -e esp32dev -t upload`

Expected:
- Session screen with a `67%` orange ring (the indicator should reach ~2/3 around).
- `resets 16:01 · 98m left` below.
- `Sonnet  412k` and `Opus  88k` lines.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/ui/screen_session.h firmware/src/ui/screen_session.cpp firmware/src/main.cpp
git commit -m "add session screen with 5h block ring"
```

---

### Task 12: Screen — All Models Today

**Files:**
- Create: `firmware/src/ui/screen_models.h`
- Create: `firmware/src/ui/screen_models.cpp`

- [ ] **Step 1: Declare and implement**

```cpp
// firmware/src/ui/screen_models.h
#pragma once

#include <lvgl.h>

#include "net/stats_types.h"

namespace cyd {

class ScreenModels {
 public:
  void build(lv_obj_t *parent);
  void update(const Stats &s);

 private:
  struct Row {
    lv_obj_t *label = nullptr;
    lv_obj_t *bar = nullptr;
    lv_obj_t *tokens = nullptr;
  };
  Row opus_, sonnet_, haiku_;
  lv_obj_t *total_ = nullptr;
  lv_obj_t *cost_ = nullptr;
};

} // namespace cyd
```

```cpp
// firmware/src/ui/screen_models.cpp
#include "ui/screen_models.h"

#include <stdio.h>
#include <algorithm>

#include "ui/theme.h"

namespace cyd {

static void build_row(lv_obj_t *parent, int y_offset, const char *name,
                      ScreenModels::Row &r) {
  r.label = lv_label_create(parent);
  lv_label_set_text(r.label, name);
  lv_obj_set_style_text_color(r.label, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(r.label, &lv_font_montserrat_14, 0);
  lv_obj_align(r.label, LV_ALIGN_TOP_LEFT, 4, y_offset);

  r.bar = lv_bar_create(parent);
  lv_obj_set_size(r.bar, 160, 14);
  lv_obj_align(r.bar, LV_ALIGN_TOP_LEFT, 60, y_offset + 2);
  lv_bar_set_range(r.bar, 0, 1000000);
  lv_obj_set_style_bg_color(r.bar, theme::c(0x222226), LV_PART_MAIN);
  lv_obj_set_style_bg_color(r.bar, theme::c(theme::blue), LV_PART_INDICATOR);
  lv_obj_set_style_radius(r.bar, 6, LV_PART_MAIN);
  lv_obj_set_style_radius(r.bar, 6, LV_PART_INDICATOR);

  r.tokens = lv_label_create(parent);
  lv_obj_set_style_text_color(r.tokens, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(r.tokens, &lv_font_montserrat_12, 0);
  lv_obj_align(r.tokens, LV_ALIGN_TOP_LEFT, 60, y_offset + 20);
}

void ScreenModels::build(lv_obj_t *parent) {
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "All Models · Today");
  lv_obj_set_style_text_color(title, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

  build_row(parent, 22, "Opus",   opus_);
  build_row(parent, 76, "Sonnet", sonnet_);
  build_row(parent, 130, "Haiku", haiku_);

  total_ = lv_label_create(parent);
  lv_obj_set_style_text_color(total_, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(total_, &lv_font_montserrat_16, 0);
  lv_obj_align(total_, LV_ALIGN_TOP_LEFT, 4, 200);

  cost_ = lv_label_create(parent);
  lv_obj_set_style_text_color(cost_, theme::c(theme::accent), 0);
  lv_obj_set_style_text_font(cost_, &lv_font_montserrat_24, 0);
  lv_obj_align(cost_, LV_ALIGN_TOP_LEFT, 4, 222);
}

static int find_tokens(const std::vector<ModelTokenRow> &rows, const char *needle) {
  for (const auto &r : rows) {
    if (r.model.find(needle) != std::string::npos) return r.tokens;
  }
  return 0;
}

void ScreenModels::update(const Stats &s) {
  int opus = find_tokens(s.models_today.by_model, "opus");
  int sonnet = find_tokens(s.models_today.by_model, "sonnet");
  int haiku = find_tokens(s.models_today.by_model, "haiku");

  // Scale bars relative to the largest, so the visual is comparative.
  int cap = std::max({opus, sonnet, haiku, 1});
  lv_bar_set_range(opus_.bar, 0, cap);
  lv_bar_set_range(sonnet_.bar, 0, cap);
  lv_bar_set_range(haiku_.bar, 0, cap);

  auto fill = [](Row &r, int tokens) {
    lv_bar_set_value(r.bar, tokens, LV_ANIM_OFF);
    char buf[24];
    snprintf(buf, sizeof(buf), "%dk tokens", tokens / 1000);
    lv_label_set_text(r.tokens, buf);
  };
  fill(opus_, opus);
  fill(sonnet_, sonnet);
  fill(haiku_, haiku);

  char buf[40];
  snprintf(buf, sizeof(buf), "%dk total", s.models_today.total_tokens / 1000);
  lv_label_set_text(total_, buf);
  snprintf(buf, sizeof(buf), "$%.2f est.", s.models_today.est_cost_usd);
  lv_label_set_text(cost_, buf);
}

} // namespace cyd
```

- [ ] **Step 2: Smoke-test in main.cpp**

```cpp
static cyd::ScreenModels models;
models.build(tv.tile(cyd::SCR_MODELS));
stub.models_today.total_tokens = 612000;
stub.models_today.est_cost_usd = 7.41;
stub.models_today.by_model = {
  {"claude-opus-4-7", 102000},
  {"claude-sonnet-4-6", 480000},
  {"claude-haiku-4-5", 30000},
};
models.update(stub);
```

- [ ] **Step 3: Flash and verify**

Run: `pio run -e esp32dev -t upload`

Expected: three blue horizontal bars (sonnet longest, opus medium, haiku short); `612k total` and `$7.41 est.` below.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/ui/screen_models.h firmware/src/ui/screen_models.cpp firmware/src/main.cpp
git commit -m "add all-models-today screen with per-model bars"
```

---

### Task 13: Screen — Sonnet weekly

**Files:**
- Create: `firmware/src/ui/screen_sonnet.h`
- Create: `firmware/src/ui/screen_sonnet.cpp`

- [ ] **Step 1: Declare and implement**

```cpp
// firmware/src/ui/screen_sonnet.h
#pragma once

#include <lvgl.h>
#include "net/stats_types.h"

namespace cyd {
class ScreenSonnet {
 public:
  void build(lv_obj_t *parent);
  void update(const Stats &s);
 private:
  lv_obj_t *bar_ = nullptr;
  lv_obj_t *pct_ = nullptr;
  lv_obj_t *amount_ = nullptr;
  lv_obj_t *pace_ = nullptr;
};
} // namespace cyd
```

```cpp
// firmware/src/ui/screen_sonnet.cpp
#include "ui/screen_sonnet.h"

#include <stdio.h>
#include "ui/theme.h"

namespace cyd {

void ScreenSonnet::build(lv_obj_t *parent) {
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Sonnet · This Week");
  lv_obj_set_style_text_color(title, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

  pct_ = lv_label_create(parent);
  lv_obj_set_style_text_color(pct_, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(pct_, &lv_font_montserrat_32, 0);
  lv_obj_align(pct_, LV_ALIGN_TOP_LEFT, 4, 24);

  bar_ = lv_bar_create(parent);
  lv_obj_set_size(bar_, 220, 18);
  lv_obj_align(bar_, LV_ALIGN_TOP_LEFT, 4, 80);
  lv_bar_set_range(bar_, 0, 100);
  lv_obj_set_style_bg_color(bar_, theme::c(0x222226), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar_, theme::c(theme::blue), LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar_, 8, LV_PART_MAIN);
  lv_obj_set_style_radius(bar_, 8, LV_PART_INDICATOR);

  amount_ = lv_label_create(parent);
  lv_obj_set_style_text_color(amount_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(amount_, &lv_font_montserrat_14, 0);
  lv_obj_align(amount_, LV_ALIGN_TOP_LEFT, 4, 110);

  pace_ = lv_label_create(parent);
  lv_obj_set_style_text_color(pace_, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(pace_, &lv_font_montserrat_16, 0);
  lv_obj_align(pace_, LV_ALIGN_TOP_LEFT, 4, 150);
}

void ScreenSonnet::update(const Stats &s) {
  char buf[40];
  snprintf(buf, sizeof(buf), "%d%%", s.sonnet.weekly_pct);
  lv_label_set_text(pct_, buf);
  lv_bar_set_value(bar_, s.sonnet.weekly_pct, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar_, theme::bar_color_for_pct(s.sonnet.weekly_pct),
                            LV_PART_INDICATOR);
  snprintf(buf, sizeof(buf), "%dk / %dk tokens",
           s.sonnet.used / 1000, s.sonnet.cap / 1000);
  lv_label_set_text(amount_, buf);

  const char *pace = "—";
  uint32_t color = theme::fg_muted;
  if (s.sonnet.pace == "ahead")    { pace = "↑ ahead of pace";    color = theme::yellow; }
  else if (s.sonnet.pace == "on_track") { pace = "→ on pace";     color = theme::green;  }
  else if (s.sonnet.pace == "behind")   { pace = "↓ behind pace"; color = theme::blue;   }
  lv_label_set_text(pace_, pace);
  lv_obj_set_style_text_color(pace_, theme::c(color), 0);
}

} // namespace cyd
```

- [ ] **Step 2: Smoke-test**

```cpp
static cyd::ScreenSonnet sonnet;
sonnet.build(tv.tile(cyd::SCR_SONNET));
stub.sonnet = {58, 1160000, 2000000, "on_track"};
sonnet.update(stub);
```

- [ ] **Step 3: Flash and verify**

Run: `pio run -e esp32dev -t upload`

Expected: `58%`, a half-filled blue bar, `1160k / 2000k tokens`, green `→ on pace` line.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/ui/screen_sonnet.h firmware/src/ui/screen_sonnet.cpp firmware/src/main.cpp
git commit -m "add Sonnet weekly budget screen"
```

---

### Task 14: Screen — Chat (empty-state aware)

**Files:**
- Create: `firmware/src/ui/screen_chat.h`
- Create: `firmware/src/ui/screen_chat.cpp`

- [ ] **Step 1: Declare and implement**

```cpp
// firmware/src/ui/screen_chat.h
#pragma once

#include <lvgl.h>
#include "net/stats_types.h"

namespace cyd {
class ScreenChat {
 public:
  void build(lv_obj_t *parent);
  void update(const Stats &s);
 private:
  lv_obj_t *count_ = nullptr;
  lv_obj_t *cap_ = nullptr;
  lv_obj_t *bar_ = nullptr;
  lv_obj_t *empty_ = nullptr;
};
} // namespace cyd
```

```cpp
// firmware/src/ui/screen_chat.cpp
#include "ui/screen_chat.h"

#include <stdio.h>
#include "ui/theme.h"

namespace cyd {

void ScreenChat::build(lv_obj_t *parent) {
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Claude.ai · Chat");
  lv_obj_set_style_text_color(title, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

  count_ = lv_label_create(parent);
  lv_obj_set_style_text_color(count_, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(count_, &lv_font_montserrat_32, 0);
  lv_obj_align(count_, LV_ALIGN_TOP_LEFT, 4, 30);
  lv_label_set_text(count_, "—");

  cap_ = lv_label_create(parent);
  lv_obj_set_style_text_color(cap_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(cap_, &lv_font_montserrat_14, 0);
  lv_obj_align(cap_, LV_ALIGN_TOP_LEFT, 4, 80);
  lv_label_set_text(cap_, "");

  bar_ = lv_bar_create(parent);
  lv_obj_set_size(bar_, 220, 14);
  lv_obj_align(bar_, LV_ALIGN_TOP_LEFT, 4, 110);
  lv_bar_set_range(bar_, 0, 100);
  lv_obj_set_style_bg_color(bar_, theme::c(0x222226), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar_, theme::c(theme::blue), LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar_, 6, LV_PART_MAIN);
  lv_obj_set_style_radius(bar_, 6, LV_PART_INDICATOR);

  empty_ = lv_label_create(parent);
  lv_obj_set_style_text_color(empty_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(empty_, &lv_font_montserrat_14, 0);
  lv_label_set_long_mode(empty_, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(empty_, 220);
  lv_obj_align(empty_, LV_ALIGN_TOP_LEFT, 4, 150);
  lv_label_set_text(empty_, "Sign in to Claude.ai\nfrom localhost:7842 to\ntrack daily chat.");
}

void ScreenChat::update(const Stats &s) {
  bool empty = (s.chat.daily_cap == 0 && s.chat.messages_today == 0);
  if (empty) {
    lv_obj_clear_flag(empty_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(bar_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(cap_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(count_, "—");
    return;
  }
  lv_obj_add_flag(empty_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(bar_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(cap_, LV_OBJ_FLAG_HIDDEN);

  char buf[24];
  snprintf(buf, sizeof(buf), "%d", s.chat.messages_today);
  lv_label_set_text(count_, buf);
  snprintf(buf, sizeof(buf), "of %d daily", s.chat.daily_cap);
  lv_label_set_text(cap_, buf);
  int pct = s.chat.daily_cap > 0
                ? (s.chat.messages_today * 100) / s.chat.daily_cap : 0;
  lv_bar_set_value(bar_, pct, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar_, theme::bar_color_for_pct(pct), LV_PART_INDICATOR);
}

} // namespace cyd
```

- [ ] **Step 2: Smoke-test the empty-state**

```cpp
static cyd::ScreenChat chat;
chat.build(tv.tile(cyd::SCR_CHAT));
// Leave stub.chat at zeros → empty-state path.
chat.update(stub);
```

- [ ] **Step 3: Flash and verify**

Run: `pio run -e esp32dev -t upload`

Expected: chat screen shows the "Sign in to Claude.ai…" empty-state message and the count reads `—`.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/ui/screen_chat.h firmware/src/ui/screen_chat.cpp firmware/src/main.cpp
git commit -m "add Claude.ai chat screen with empty-state copy"
```

---

### Task 15: Screen — Routines

**Files:**
- Create: `firmware/src/ui/screen_routines.h`
- Create: `firmware/src/ui/screen_routines.cpp`

- [ ] **Step 1: Declare and implement**

```cpp
// firmware/src/ui/screen_routines.h
#pragma once

#include <lvgl.h>
#include "net/stats_types.h"

namespace cyd {
class ScreenRoutines {
 public:
  void build(lv_obj_t *parent);
  void update(const Stats &s);
 private:
  static constexpr int kMaxRows = 5;
  struct Row {
    lv_obj_t *name = nullptr;
    lv_obj_t *pill = nullptr;
    lv_obj_t *when = nullptr;
  };
  Row rows_[kMaxRows];
  lv_obj_t *empty_ = nullptr;
};
} // namespace cyd
```

```cpp
// firmware/src/ui/screen_routines.cpp
#include "ui/screen_routines.h"

#include "ui/theme.h"

namespace cyd {

void ScreenRoutines::build(lv_obj_t *parent) {
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Routines");
  lv_obj_set_style_text_color(title, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

  for (int i = 0; i < kMaxRows; ++i) {
    int y = 22 + i * 38;
    rows_[i].name = lv_label_create(parent);
    lv_obj_set_style_text_color(rows_[i].name, theme::c(theme::fg), 0);
    lv_obj_set_style_text_font(rows_[i].name, &lv_font_montserrat_14, 0);
    lv_obj_align(rows_[i].name, LV_ALIGN_TOP_LEFT, 4, y);
    lv_obj_add_flag(rows_[i].name, LV_OBJ_FLAG_HIDDEN);

    rows_[i].pill = lv_obj_create(parent);
    lv_obj_set_size(rows_[i].pill, 56, 18);
    lv_obj_align(rows_[i].pill, LV_ALIGN_TOP_RIGHT, -4, y - 2);
    lv_obj_set_style_radius(rows_[i].pill, 9, 0);
    lv_obj_set_style_border_width(rows_[i].pill, 0, 0);
    lv_obj_clear_flag(rows_[i].pill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(rows_[i].pill, LV_OBJ_FLAG_HIDDEN);

    auto *pill_label = lv_label_create(rows_[i].pill);
    lv_obj_set_style_text_color(pill_label, theme::c(theme::bg), 0);
    lv_obj_set_style_text_font(pill_label, &lv_font_montserrat_12, 0);
    lv_obj_center(pill_label);
    lv_obj_set_user_data(rows_[i].pill, pill_label);

    rows_[i].when = lv_label_create(parent);
    lv_obj_set_style_text_color(rows_[i].when, theme::c(theme::fg_muted), 0);
    lv_obj_set_style_text_font(rows_[i].when, &lv_font_montserrat_12, 0);
    lv_obj_align(rows_[i].when, LV_ALIGN_TOP_LEFT, 4, y + 18);
    lv_obj_add_flag(rows_[i].when, LV_OBJ_FLAG_HIDDEN);
  }

  empty_ = lv_label_create(parent);
  lv_obj_set_style_text_color(empty_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(empty_, &lv_font_montserrat_14, 0);
  lv_label_set_long_mode(empty_, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(empty_, 220);
  lv_obj_align(empty_, LV_ALIGN_TOP_LEFT, 4, 90);
  lv_label_set_text(empty_, "No routines yet.\nRun `claude routines add`\nto get started.");
}

void ScreenRoutines::update(const Stats &s) {
  bool any = !s.routines.empty();
  if (any) lv_obj_add_flag(empty_, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(empty_, LV_OBJ_FLAG_HIDDEN);

  for (int i = 0; i < kMaxRows; ++i) {
    bool show = i < (int)s.routines.size();
    auto vis = [show](lv_obj_t *o) {
      if (show) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
      else      lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    };
    vis(rows_[i].name);
    vis(rows_[i].pill);
    vis(rows_[i].when);
    if (!show) continue;

    const auto &r = s.routines[i];
    lv_label_set_text(rows_[i].name, r.name.c_str());
    lv_obj_set_style_bg_color(rows_[i].pill,
                              theme::status_pill_for(r.status.c_str()), 0);
    auto *pill_label =
        static_cast<lv_obj_t *>(lv_obj_get_user_data(rows_[i].pill));
    lv_label_set_text(pill_label, r.status.c_str());

    char w[40];
    snprintf(w, sizeof(w), "last %s · next %s",
             r.last_run.c_str(), r.next_run.c_str());
    lv_label_set_text(rows_[i].when, w);
  }
}

} // namespace cyd
```

- [ ] **Step 2: Smoke-test**

```cpp
static cyd::ScreenRoutines routines;
routines.build(tv.tile(cyd::SCR_ROUTINES));
stub.routines = {
  {"babysit-prs", "ok", "12:00", "17:00"},
  {"morning-digest", "slow", "09:01", "—"},
};
routines.update(stub);
```

- [ ] **Step 3: Flash and verify**

Run: `pio run -e esp32dev -t upload`

Expected: two rows; `babysit-prs` with green `ok` pill, `morning-digest` with yellow `slow` pill; subtitle under each shows last/next.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/ui/screen_routines.h firmware/src/ui/screen_routines.cpp firmware/src/main.cpp
git commit -m "add routines screen with status pills"
```

---

### Task 16: Screen — Budgets

**Files:**
- Create: `firmware/src/ui/screen_budgets.h`
- Create: `firmware/src/ui/screen_budgets.cpp`

- [ ] **Step 1: Declare and implement**

```cpp
// firmware/src/ui/screen_budgets.h
#pragma once

#include <lvgl.h>
#include "net/stats_types.h"

namespace cyd {
class ScreenBudgets {
 public:
  void build(lv_obj_t *parent);
  void update(const Stats &s);
 private:
  struct Row {
    lv_obj_t *label = nullptr;
    lv_obj_t *bar = nullptr;
    lv_obj_t *pct = nullptr;
  };
  Row code_all_, code_opus_, chat_;
  lv_obj_t *plan_ = nullptr;
  lv_obj_t *resets_ = nullptr;
  lv_obj_t *warn_ = nullptr;
};
} // namespace cyd
```

```cpp
// firmware/src/ui/screen_budgets.cpp
#include "ui/screen_budgets.h"

#include <stdio.h>
#include "ui/theme.h"

namespace cyd {

static void build_row(lv_obj_t *parent, int y, const char *name,
                      ScreenBudgets::Row &r) {
  r.label = lv_label_create(parent);
  lv_label_set_text(r.label, name);
  lv_obj_set_style_text_color(r.label, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(r.label, &lv_font_montserrat_14, 0);
  lv_obj_align(r.label, LV_ALIGN_TOP_LEFT, 4, y);

  r.bar = lv_bar_create(parent);
  lv_obj_set_size(r.bar, 150, 14);
  lv_obj_align(r.bar, LV_ALIGN_TOP_LEFT, 60, y + 2);
  lv_bar_set_range(r.bar, 0, 100);
  lv_obj_set_style_bg_color(r.bar, theme::c(0x222226), LV_PART_MAIN);
  lv_obj_set_style_bg_color(r.bar, theme::c(theme::blue), LV_PART_INDICATOR);
  lv_obj_set_style_radius(r.bar, 6, LV_PART_MAIN);
  lv_obj_set_style_radius(r.bar, 6, LV_PART_INDICATOR);

  r.pct = lv_label_create(parent);
  lv_obj_set_style_text_color(r.pct, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(r.pct, &lv_font_montserrat_12, 0);
  lv_obj_align(r.pct, LV_ALIGN_TOP_RIGHT, -4, y + 2);
}

void ScreenBudgets::build(lv_obj_t *parent) {
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Budgets · This Week");
  lv_obj_set_style_text_color(title, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

  build_row(parent, 24, "Code",  code_all_);
  build_row(parent, 60, "Opus",  code_opus_);
  build_row(parent, 96, "Chat",  chat_);

  plan_ = lv_label_create(parent);
  lv_obj_set_style_text_color(plan_, theme::c(theme::accent), 0);
  lv_obj_set_style_text_font(plan_, &lv_font_montserrat_16, 0);
  lv_obj_align(plan_, LV_ALIGN_TOP_LEFT, 4, 150);

  resets_ = lv_label_create(parent);
  lv_obj_set_style_text_color(resets_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(resets_, &lv_font_montserrat_14, 0);
  lv_obj_align(resets_, LV_ALIGN_TOP_LEFT, 4, 178);

  warn_ = lv_label_create(parent);
  lv_obj_set_style_text_color(warn_, theme::c(theme::bg), 0);
  lv_obj_set_style_text_font(warn_, &lv_font_montserrat_12, 0);
  lv_obj_set_style_bg_color(warn_, theme::c(theme::red), 0);
  lv_obj_set_style_bg_opa(warn_, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(warn_, 4, 0);
  lv_obj_set_style_radius(warn_, 6, 0);
  lv_obj_align(warn_, LV_ALIGN_TOP_LEFT, 4, 210);
  lv_obj_add_flag(warn_, LV_OBJ_FLAG_HIDDEN);
}

static void set_row(ScreenBudgets::Row &r, int pct) {
  lv_bar_set_value(r.bar, pct, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(r.bar, theme::bar_color_for_pct(pct), LV_PART_INDICATOR);
  char b[8];
  snprintf(b, sizeof(b), "%d%%", pct);
  lv_label_set_text(r.pct, b);
}

void ScreenBudgets::update(const Stats &s) {
  set_row(code_all_, s.budgets.code_all);
  set_row(code_opus_, s.budgets.code_opus);
  set_row(chat_, s.budgets.chat);

  lv_label_set_text(plan_, s.budgets.plan.empty() ? "—" : s.budgets.plan.c_str());
  char r[40];
  snprintf(r, sizeof(r), "resets in %s", s.budgets.resets_in.c_str());
  lv_label_set_text(resets_, r);

  int worst = std::max({s.budgets.code_all, s.budgets.code_opus, s.budgets.chat});
  if (worst >= 85) {
    char w[40];
    snprintf(w, sizeof(w), "  %d%% used — slow down  ", worst);
    lv_label_set_text(warn_, w);
    lv_obj_clear_flag(warn_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(warn_, LV_OBJ_FLAG_HIDDEN);
  }
}

} // namespace cyd
```

- [ ] **Step 2: Smoke-test**

```cpp
static cyd::ScreenBudgets budgets;
budgets.build(tv.tile(cyd::SCR_BUDGETS));
stub.budgets = {71, 89, 22, "max-20x", "3d19h"};
budgets.update(stub);
```

- [ ] **Step 3: Flash and verify**

Run: `pio run -e esp32dev -t upload`

Expected: three bars; Opus bar in red because 89 ≥ 85; `max-20x` in orange; `resets in 3d19h`; red warning pill at the bottom reading `89% used — slow down`.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/ui/screen_budgets.h firmware/src/ui/screen_budgets.cpp firmware/src/main.cpp
git commit -m "add weekly budgets screen with danger threshold warning"
```

---

### Task 17: Pre-pairing screens (provision / discover / pair)

**Files:**
- Create: `firmware/src/ui/provision_screen.h` / `.cpp`
- Create: `firmware/src/ui/discover_screen.h` / `.cpp`
- Create: `firmware/src/ui/pair_screen.h` / `.cpp`

These three screens are shown before the tileview ever appears. They reuse the same chrome (status bar + footer pips can stay attached but the pips will all be muted).

- [ ] **Step 1: Provision screen**

```cpp
// firmware/src/ui/provision_screen.h
#pragma once

#include <lvgl.h>

namespace cyd {
class ProvisionScreen {
 public:
  void build(lv_obj_t *parent);
  void set_ap_ssid(const char *ssid);
 private:
  lv_obj_t *ssid_label_ = nullptr;
};
} // namespace cyd
```

```cpp
// firmware/src/ui/provision_screen.cpp
#include "ui/provision_screen.h"

#include "ui/theme.h"

namespace cyd {

void ProvisionScreen::build(lv_obj_t *parent) {
  theme::apply_screen_styles(parent);
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Connect to WiFi");
  lv_obj_set_style_text_color(title, theme::c(theme::accent), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

  auto *step = lv_label_create(parent);
  lv_label_set_long_mode(step, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(step, 220);
  lv_label_set_text(step,
      "1. On your phone or laptop,\n   join the WiFi network:");
  lv_obj_set_style_text_color(step, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(step, &lv_font_montserrat_14, 0);
  lv_obj_align(step, LV_ALIGN_TOP_LEFT, 8, 80);

  ssid_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(ssid_label_, theme::c(theme::accent), 0);
  lv_obj_set_style_text_font(ssid_label_, &lv_font_montserrat_24, 0);
  lv_obj_align(ssid_label_, LV_ALIGN_TOP_MID, 0, 140);
  lv_label_set_text(ssid_label_, "ClaudeMonitor-XXXX");

  auto *finish = lv_label_create(parent);
  lv_label_set_long_mode(finish, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(finish, 220);
  lv_label_set_text(finish,
      "2. The captive portal opens.\n3. Pick your home WiFi,\n   enter the password.");
  lv_obj_set_style_text_color(finish, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(finish, &lv_font_montserrat_14, 0);
  lv_obj_align(finish, LV_ALIGN_TOP_LEFT, 8, 200);
}

void ProvisionScreen::set_ap_ssid(const char *ssid) {
  lv_label_set_text(ssid_label_, ssid);
}

} // namespace cyd
```

- [ ] **Step 2: Discover screen**

```cpp
// firmware/src/ui/discover_screen.h
#pragma once
#include <lvgl.h>
namespace cyd {
class DiscoverScreen {
 public:
  void build(lv_obj_t *parent);
  void set_status(const char *status);
 private:
  lv_obj_t *status_ = nullptr;
};
} // namespace cyd
```

```cpp
// firmware/src/ui/discover_screen.cpp
#include "ui/discover_screen.h"
#include "ui/theme.h"

namespace cyd {

void DiscoverScreen::build(lv_obj_t *parent) {
  theme::apply_screen_styles(parent);
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Looking for daemon");
  lv_obj_set_style_text_color(title, theme::c(theme::accent), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 60);

  status_ = lv_label_create(parent);
  lv_label_set_long_mode(status_, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(status_, 220);
  lv_obj_set_style_text_color(status_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(status_, &lv_font_montserrat_14, 0);
  lv_obj_align(status_, LV_ALIGN_TOP_MID, 0, 130);
  lv_label_set_text(status_,
      "Make sure the daemon is\nrunning on your Mac:\n\n  brew install cydmonitor");
}

void DiscoverScreen::set_status(const char *s) {
  if (s) lv_label_set_text(status_, s);
}

} // namespace cyd
```

- [ ] **Step 3: Pair screen with tap-to-confirm**

```cpp
// firmware/src/ui/pair_screen.h
#pragma once
#include <lvgl.h>

namespace cyd {
class PairScreen {
 public:
  using ConfirmCb = void (*)();
  void build(lv_obj_t *parent);
  void set_code(const char *code);
  void set_host(const char *host);
  void on_confirm(ConfirmCb cb) { cb_ = cb; }
 private:
  lv_obj_t *code_label_ = nullptr;
  lv_obj_t *host_label_ = nullptr;
  lv_obj_t *button_ = nullptr;
  ConfirmCb cb_ = nullptr;
  static void btn_event(lv_event_t *e);
};
} // namespace cyd
```

```cpp
// firmware/src/ui/pair_screen.cpp
#include "ui/pair_screen.h"
#include "ui/theme.h"

namespace cyd {

void PairScreen::build(lv_obj_t *parent) {
  theme::apply_screen_styles(parent);

  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Pair with daemon");
  lv_obj_set_style_text_color(title, theme::c(theme::accent), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

  host_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(host_label_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(host_label_, &lv_font_montserrat_14, 0);
  lv_obj_align(host_label_, LV_ALIGN_TOP_MID, 0, 56);
  lv_label_set_text(host_label_, "");

  code_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(code_label_, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(code_label_, &lv_font_montserrat_32, 0);
  lv_obj_align(code_label_, LV_ALIGN_TOP_MID, 0, 90);
  lv_label_set_text(code_label_, "----");

  auto *hint = lv_label_create(parent);
  lv_obj_set_style_text_color(hint, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
  lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 138);
  lv_label_set_text(hint, "Check the daemon's tray icon\nor run `cydmonitor status`");

  button_ = lv_btn_create(parent);
  lv_obj_set_size(button_, 200, 48);
  lv_obj_align(button_, LV_ALIGN_TOP_MID, 0, 200);
  lv_obj_set_style_bg_color(button_, theme::c(theme::accent), 0);
  lv_obj_set_style_radius(button_, 24, 0);
  auto *btn_label = lv_label_create(button_);
  lv_label_set_text(btn_label, "Confirm");
  lv_obj_set_style_text_color(btn_label, theme::c(theme::bg), 0);
  lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_16, 0);
  lv_obj_center(btn_label);
  lv_obj_add_event_cb(button_, &PairScreen::btn_event, LV_EVENT_CLICKED, this);
}

void PairScreen::set_code(const char *c) {
  if (c) lv_label_set_text(code_label_, c);
}

void PairScreen::set_host(const char *h) {
  if (h) lv_label_set_text(host_label_, h);
}

void PairScreen::btn_event(lv_event_t *e) {
  auto *self = static_cast<PairScreen *>(lv_event_get_user_data(e));
  if (self && self->cb_) self->cb_();
}

} // namespace cyd
```

- [ ] **Step 4: Smoke-test (provision + pair only)**

In `main.cpp`, temporarily replace tileview wiring with:

```cpp
static cyd::ProvisionScreen prov;
prov.build(scr);
prov.set_ap_ssid("ClaudeMonitor-7A23");
```

Flash, confirm the provision screen renders. Then swap to:

```cpp
static cyd::PairScreen pair;
pair.build(scr);
pair.set_host("krizzo's MacBook");
pair.set_code("4719");
pair.on_confirm([]() { Serial.println("confirm tapped"); });
```

Flash, verify `Confirm` button responds to a tap (log line appears).

- [ ] **Step 5: Restore tileview wiring**

Revert the temporary main.cpp changes so the tileview build is back. The three pre-pairing screens are now ready for the state machine driver in Task 21.

- [ ] **Step 6: Commit**

```bash
git add firmware/src/ui/provision_screen.h firmware/src/ui/provision_screen.cpp \
        firmware/src/ui/discover_screen.h firmware/src/ui/discover_screen.cpp \
        firmware/src/ui/pair_screen.h firmware/src/ui/pair_screen.cpp \
        firmware/src/main.cpp
git commit -m "add provision/discover/pair pre-pairing screens"
```

---

### Task 18: WiFi onboarding (WiFiManager + captive portal)

**Files:**
- Create: `firmware/src/net/wifi_onboarding.h`
- Create: `firmware/src/net/wifi_onboarding.cpp`

- [ ] **Step 1: Declare the onboarding module**

```cpp
// firmware/src/net/wifi_onboarding.h
#pragma once

#include <string>

namespace cyd {

class WifiOnboarding {
 public:
  // Returns the AP SSID we'll advertise, e.g. "ClaudeMonitor-7A23".
  std::string ap_ssid() const;

  // Try saved credentials with a 12s timeout. Returns true if connected.
  bool try_saved(const std::string &ssid, const std::string &psk);

  // Block until the user finishes the captive portal flow. Returns the SSID
  // and PSK chosen so the caller can save them to NVS. The portal page is
  // titled "Claude Monitor".
  bool run_portal(std::string &out_ssid, std::string &out_psk);
};

} // namespace cyd
```

- [ ] **Step 2: Implement using WiFiManager**

```cpp
// firmware/src/net/wifi_onboarding.cpp
#include "net/wifi_onboarding.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>

namespace cyd {

namespace {
std::string make_ap_name() {
  uint64_t mac = ESP.getEfuseMac();
  char buf[24];
  snprintf(buf, sizeof(buf), "ClaudeMonitor-%04X",
           (unsigned)(mac & 0xFFFF));
  return buf;
}
}

std::string WifiOnboarding::ap_ssid() const {
  static std::string n = make_ap_name();
  return n;
}

bool WifiOnboarding::try_saved(const std::string &ssid, const std::string &psk) {
  if (ssid.empty()) return false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), psk.c_str());
  uint32_t deadline = millis() + 12000;
  while (millis() < deadline) {
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(200);
  }
  return false;
}

bool WifiOnboarding::run_portal(std::string &out_ssid, std::string &out_psk) {
  WiFiManager wm;
  wm.setTitle("Claude Monitor");
  wm.setConfigPortalTimeout(0);                 // wait forever
  wm.setBreakAfterConfig(true);                 // exit once creds saved
  if (!wm.startConfigPortal(ap_ssid().c_str())) {
    return false;
  }
  out_ssid = WiFi.SSID().c_str();
  out_psk  = WiFi.psk().c_str();
  return WiFi.status() == WL_CONNECTED;
}

} // namespace cyd
```

- [ ] **Step 3: Verify build**

Run: `cd firmware && pio run -e esp32dev`
Expected: build succeeds.

(End-to-end onboarding is exercised in Task 21 once the state machine glues this to `ProvisionScreen`.)

- [ ] **Step 4: Commit**

```bash
git add firmware/src/net/wifi_onboarding.h firmware/src/net/wifi_onboarding.cpp
git commit -m "add WiFiManager-backed WiFi onboarding"
```

---

### Task 19: mDNS daemon discovery

**Files:**
- Create: `firmware/src/net/mdns_discover.h`
- Create: `firmware/src/net/mdns_discover.cpp`

- [ ] **Step 1: Declare the discoverer**

```cpp
// firmware/src/net/mdns_discover.h
#pragma once

#include <string>

namespace cyd {

struct DaemonAddr {
  std::string hostname;   // e.g., "krizzos-mbp.local"
  std::string display;    // friendly name from TXT host=...
  uint16_t port = 0;
};

class MdnsDiscover {
 public:
  // One-shot blocking query for _claudeusage._tcp. Returns false if no
  // service found within the timeout (see kMdnsQueryMs in app_config.h).
  bool find(DaemonAddr &out);
};

} // namespace cyd
```

- [ ] **Step 2: Implement**

```cpp
// firmware/src/net/mdns_discover.cpp
#include "net/mdns_discover.h"

#include <ESPmDNS.h>

#include "app/app_config.h"

namespace cyd {

bool MdnsDiscover::find(DaemonAddr &out) {
  if (!MDNS.begin("cydmonitor-client")) return false;
  int n = MDNS.queryService("claudeusage", "tcp");
  if (n <= 0) return false;

  out.hostname = std::string(MDNS.hostname(0).c_str()) + ".local";
  out.port = MDNS.port(0);
  // TXT record: host=<friendly>
  String friendly = MDNS.txt(0, "host");
  out.display = friendly.length() > 0 ? friendly.c_str() : out.hostname;
  return true;
}

} // namespace cyd
```

- [ ] **Step 3: Build**

Run: `cd firmware && pio run -e esp32dev`
Expected: build succeeds.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/net/mdns_discover.h firmware/src/net/mdns_discover.cpp
git commit -m "add mDNS discovery for _claudeusage._tcp"
```

---

### Task 20: Pairing + stats HTTP clients

**Files:**
- Create: `firmware/src/net/pairing_client.h` / `.cpp`
- Create: `firmware/src/net/stats_client.h` / `.cpp`

- [ ] **Step 1: Pairing client header**

```cpp
// firmware/src/net/pairing_client.h
#pragma once

#include <string>

namespace cyd {

class PairingClient {
 public:
  // POST /v1/pair-init {cyd_id:"…"} → returns 4-digit code on success.
  bool init(const std::string &base_url, const std::string &cyd_id,
            std::string &out_code);

  // POST /v1/pair-verify {cyd_id, code, name} → returns 64-char hex token.
  bool verify(const std::string &base_url, const std::string &cyd_id,
              const std::string &code, const std::string &name,
              std::string &out_token);
};

// "CYD-XXYYZZ" derived from the bottom 3 bytes of the MAC.
std::string device_cyd_id();
std::string device_name();

} // namespace cyd
```

- [ ] **Step 2: Pairing client implementation**

```cpp
// firmware/src/net/pairing_client.cpp
#include "net/pairing_client.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "app/app_config.h"

namespace cyd {

namespace {
std::string mac_suffix() {
  uint64_t m = ESP.getEfuseMac();
  char b[8];
  snprintf(b, sizeof(b), "%06X", (unsigned)(m & 0xFFFFFF));
  return b;
}
}

std::string device_cyd_id() {
  return std::string("CYD-") + mac_suffix();
}

std::string device_name() {
  return std::string("cyd-") + mac_suffix();
}

static bool post_json(const std::string &url, const std::string &body,
                      std::string &out) {
  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  if (!http.begin(url.c_str())) return false;
  http.addHeader("Content-Type", "application/json");
  int code = http.POST((uint8_t *)body.data(), body.size());
  if (code < 200 || code >= 300) { http.end(); return false; }
  out = http.getString().c_str();
  http.end();
  return true;
}

bool PairingClient::init(const std::string &base_url,
                         const std::string &cyd_id,
                         std::string &out_code) {
  JsonDocument req;
  req["cyd_id"] = cyd_id;
  std::string body;
  serializeJson(req, body);
  std::string resp;
  if (!post_json(base_url + "/v1/pair-init", body, resp)) return false;
  JsonDocument doc;
  if (deserializeJson(doc, resp)) return false;
  const char *c = doc["code"] | "";
  if (!*c) return false;
  out_code = c;
  return true;
}

bool PairingClient::verify(const std::string &base_url,
                           const std::string &cyd_id,
                           const std::string &code,
                           const std::string &name,
                           std::string &out_token) {
  JsonDocument req;
  req["cyd_id"] = cyd_id;
  req["code"]   = code;
  req["name"]   = name;
  std::string body;
  serializeJson(req, body);
  std::string resp;
  if (!post_json(base_url + "/v1/pair-verify", body, resp)) return false;
  JsonDocument doc;
  if (deserializeJson(doc, resp)) return false;
  const char *t = doc["token"] | "";
  if (!*t) return false;
  out_token = t;
  return true;
}

} // namespace cyd
```

- [ ] **Step 3: Stats client header**

```cpp
// firmware/src/net/stats_client.h
#pragma once

#include <string>

#include "net/stats_types.h"

namespace cyd {

class StatsClient {
 public:
  // GET /v1/stats?screens=… with Authorization: Bearer <token>. Returns true
  // on 200 + valid JSON. mask bit i → SCR enum index i.
  bool fetch(const std::string &base_url, const std::string &token,
             uint8_t screen_mask, Stats &out);
};

} // namespace cyd
```

- [ ] **Step 4: Stats client implementation**

```cpp
// firmware/src/net/stats_client.cpp
#include "net/stats_client.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "app/app_config.h"
#include "net/stats_parser.h"

namespace cyd {

static const char *kScreenNames[] = {
    "session", "models", "sonnet", "chat", "routines", "budgets"
};

static std::string mask_to_query(uint8_t mask) {
  std::string q;
  for (int i = 0; i < SCR_COUNT; ++i) {
    if (!(mask & (1 << i))) continue;
    if (!q.empty()) q += ",";
    q += kScreenNames[i];
  }
  return q;
}

bool StatsClient::fetch(const std::string &base_url, const std::string &token,
                        uint8_t screen_mask, Stats &out) {
  std::string url = base_url + "/v1/stats?screens=" + mask_to_query(screen_mask);
  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  if (!http.begin(url.c_str())) return false;
  http.addHeader("Authorization", std::string("Bearer " + token).c_str());
  http.addHeader("Accept-Encoding", "gzip");
  int code = http.GET();
  if (code != 200) { http.end(); return false; }
  String body = http.getString();
  http.end();
  return parse_stats(body.c_str(), body.length(), out);
}

} // namespace cyd
```

- [ ] **Step 5: Build**

Run: `cd firmware && pio run -e esp32dev`
Expected: build succeeds.

- [ ] **Step 6: Commit**

```bash
git add firmware/src/net/pairing_client.h firmware/src/net/pairing_client.cpp \
        firmware/src/net/stats_client.h firmware/src/net/stats_client.cpp
git commit -m "add pairing + stats HTTP clients"
```

---

### Task 21: Glue the state machine to UI + I/O

**Files:**
- Create: `firmware/src/app/app_loop.h`
- Create: `firmware/src/app/app_loop.cpp`
- Modify: `firmware/src/main.cpp`

This task wires everything together: the state machine drives which screen is shown and which I/O is performed. The main loop becomes a thin tick.

- [ ] **Step 1: Declare the app driver**

```cpp
// firmware/src/app/app_loop.h
#pragma once

namespace cyd {

// Initialises display/touch/lvgl, builds the persistent chrome, loads NVS,
// and drops into the state machine. Call once from setup().
void app_init();

// Drive LVGL, run state-machine ticks, kick off network I/O on cadence.
// Call from loop().
void app_tick();

} // namespace cyd
```

- [ ] **Step 2: Implement the driver**

```cpp
// firmware/src/app/app_loop.cpp
#include "app/app_loop.h"

#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>

#include "app/app_config.h"
#include "app/state_machine.h"
#include "hw/display.h"
#include "hw/nvs.h"
#include "hw/touch.h"
#include "net/mdns_discover.h"
#include "net/pairing_client.h"
#include "net/stats_client.h"
#include "net/wifi_onboarding.h"
#include "ui/chrome.h"
#include "ui/discover_screen.h"
#include "ui/lvgl_glue.h"
#include "ui/pair_screen.h"
#include "ui/provision_screen.h"
#include "ui/screen_budgets.h"
#include "ui/screen_chat.h"
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
Nvs nvs_;
Chrome chrome_;
Tileview tileview_;
ScreenSession scr_session_;
ScreenModels scr_models_;
ScreenSonnet scr_sonnet_;
ScreenChat scr_chat_;
ScreenRoutines scr_routines_;
ScreenBudgets scr_budgets_;
ProvisionScreen prov_;
DiscoverScreen disc_;
PairScreen pair_;
WifiOnboarding wifi_;
MdnsDiscover mdns_;
PairingClient pairing_;
StatsClient stats_client_;
Stats last_stats_;
std::string daemon_base_url_;
std::string daemon_display_;
std::string pending_code_;
uint32_t next_poll_at_ = 0;
uint32_t backoff_ms_ = kBackoffStartMs;
bool confirm_pressed_ = false;

lv_obj_t *root_ = nullptr;
lv_obj_t *pre_pairing_layer_ = nullptr;   // hosts prov/disc/pair
lv_obj_t *main_layer_ = nullptr;          // hosts tileview + chrome

void show_layer(lv_obj_t *layer) {
  lv_obj_add_flag(pre_pairing_layer_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(main_layer_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(layer, LV_OBJ_FLAG_HIDDEN);
}

void render_state(State s) {
  if (s == State::PROVISION || s == State::DISCOVER || s == State::PAIR) {
    show_layer(pre_pairing_layer_);
    if (s == State::PROVISION) {
      prov_.set_ap_ssid(wifi_.ap_ssid().c_str());
      lv_obj_clear_flag(lv_obj_get_child(pre_pairing_layer_, 0), LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(lv_obj_get_child(pre_pairing_layer_, 1), LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(lv_obj_get_child(pre_pairing_layer_, 2), LV_OBJ_FLAG_HIDDEN);
    } else if (s == State::DISCOVER) {
      lv_obj_add_flag(lv_obj_get_child(pre_pairing_layer_, 0), LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(lv_obj_get_child(pre_pairing_layer_, 1), LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(lv_obj_get_child(pre_pairing_layer_, 2), LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(lv_obj_get_child(pre_pairing_layer_, 0), LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(lv_obj_get_child(pre_pairing_layer_, 1), LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(lv_obj_get_child(pre_pairing_layer_, 2), LV_OBJ_FLAG_HIDDEN);
      pair_.set_host(daemon_display_.c_str());
      pair_.set_code(pending_code_.c_str());
    }
  } else {
    show_layer(main_layer_);
  }
}

void apply_event(Event e) {
  State next = next_state(current_state, e, ctx_);
  if (next != current_state) {
    Serial.printf("state: %d → %d\n", (int)current_state, (int)next);
    current_state = next;
    render_state(current_state);
  }
}

void on_screen_change(Screen s) {
  chrome_.set_active_screen(s);
  next_poll_at_ = 0;   // refetch with new neighbor mask immediately
}

void on_pair_confirm() { confirm_pressed_ = true; }

void update_all_screens(const Stats &s) {
  scr_session_.update(s);
  scr_models_.update(s);
  scr_sonnet_.update(s);
  scr_chat_.update(s);
  scr_routines_.update(s);
  scr_budgets_.update(s);
  chrome_.set_health(s.stale ? 1 : 0);
}

void perform_provision() {
  std::string ssid, psk;
  if (wifi_.run_portal(ssid, psk)) {
    nvs_.save_wifi(ssid, psk);
    ctx_.have_wifi_creds = true;
    apply_event(Event::WIFI_OK);
  }
}

void perform_discover() {
  // Try a saved hostname first.
  std::string host = nvs_.daemon_host();
  if (!host.empty()) {
    daemon_base_url_ = "http://" + host;
    daemon_display_ = host;
    apply_event(Event::DAEMON_FOUND);
    return;
  }
  DaemonAddr addr;
  if (!mdns_.find(addr)) {
    delay(kMdnsQueryMs);
    return;
  }
  char hp[96];
  snprintf(hp, sizeof(hp), "%s:%u", addr.hostname.c_str(), addr.port);
  daemon_base_url_ = std::string("http://") + hp;
  daemon_display_ = addr.display;
  nvs_.save_daemon_host(hp);
  apply_event(Event::DAEMON_FOUND);
}

void perform_pair() {
  if (pending_code_.empty()) {
    if (!pairing_.init(daemon_base_url_, device_cyd_id(), pending_code_)) {
      delay(2000);
      return;
    }
    render_state(current_state);  // refresh code on screen
  }
  if (!confirm_pressed_) return;
  confirm_pressed_ = false;
  std::string token;
  if (pairing_.verify(daemon_base_url_, device_cyd_id(),
                      pending_code_, device_name(), token)) {
    nvs_.save_token(token);
    ctx_.have_token = true;
    pending_code_.clear();
    apply_event(Event::PAIR_CONFIRMED);
  } else {
    pending_code_.clear();
    apply_event(Event::PAIR_FAILED);
  }
}

void perform_poll() {
  uint32_t now = millis();
  if (now < next_poll_at_) return;

  Stats fresh;
  std::string token = nvs_.token();
  if (stats_client_.fetch(daemon_base_url_, token,
                          tileview_.neighbor_mask(), fresh)) {
    last_stats_ = fresh;
    last_stats_.stale = false;
    update_all_screens(last_stats_);
    backoff_ms_ = kBackoffStartMs;
    bool active_is_hot = (tileview_.active() == SCR_SESSION ||
                          tileview_.active() == SCR_MODELS);
    next_poll_at_ = now + (active_is_hot ? kActivePollMs : kIdlePollMs);
    apply_event(Event::DAEMON_RECOVERED);
  } else {
    last_stats_.stale = true;
    update_all_screens(last_stats_);
    next_poll_at_ = now + backoff_ms_;
    backoff_ms_ = std::min<uint32_t>(backoff_ms_ * 2, kBackoffMaxMs);
    chrome_.set_health(2);
    apply_event(Event::DAEMON_UNREACHABLE);
  }
}

} // namespace

void app_init() {
  display().init();
  display().setRotation(0);
  display().setBrightness(200);
  touch().probe_and_init();
  lvgl_init();
  nvs_.begin();

  ctx_.have_wifi_creds = nvs_.has_wifi_creds();
  ctx_.have_token      = nvs_.has_token();

  root_ = lv_screen_active();
  lv_obj_set_style_bg_color(root_, theme::c(theme::bg), 0);
  chrome_.attach(root_);

  pre_pairing_layer_ = lv_obj_create(root_);
  lv_obj_set_size(pre_pairing_layer_, 240, 320 - kStatusBarHeight - kFooterHeight);
  lv_obj_align(pre_pairing_layer_, LV_ALIGN_TOP_MID, 0, kStatusBarHeight);
  lv_obj_set_style_bg_color(pre_pairing_layer_, theme::c(theme::bg), 0);
  lv_obj_set_style_border_width(pre_pairing_layer_, 0, 0);
  lv_obj_clear_flag(pre_pairing_layer_, LV_OBJ_FLAG_SCROLLABLE);
  // Children: 0=prov, 1=disc, 2=pair (referenced by index in render_state).
  prov_.build(lv_obj_create(pre_pairing_layer_));
  disc_.build(lv_obj_create(pre_pairing_layer_));
  pair_.build(lv_obj_create(pre_pairing_layer_));
  pair_.on_confirm(&on_pair_confirm);

  main_layer_ = lv_obj_create(root_);
  lv_obj_set_size(main_layer_, 240, 320 - kStatusBarHeight - kFooterHeight);
  lv_obj_align(main_layer_, LV_ALIGN_TOP_MID, 0, kStatusBarHeight);
  lv_obj_set_style_bg_color(main_layer_, theme::c(theme::bg), 0);
  lv_obj_set_style_border_width(main_layer_, 0, 0);
  lv_obj_clear_flag(main_layer_, LV_OBJ_FLAG_SCROLLABLE);

  tileview_.attach(main_layer_);
  tileview_.on_change(&on_screen_change);
  scr_session_.build(tileview_.tile(SCR_SESSION));
  scr_models_.build(tileview_.tile(SCR_MODELS));
  scr_sonnet_.build(tileview_.tile(SCR_SONNET));
  scr_chat_.build(tileview_.tile(SCR_CHAT));
  scr_routines_.build(tileview_.tile(SCR_ROUTINES));
  scr_budgets_.build(tileview_.tile(SCR_BUDGETS));

  // Initial transition from BOOT.
  apply_event(Event::TICK);

  // Auto-reconnect with saved credentials.
  if (current_state == State::DISCOVER || current_state == State::POLL_RENDER) {
    if (!wifi_.try_saved(nvs_.wifi_ssid(), nvs_.wifi_psk())) {
      // Fall back to PROVISION if creds no longer valid.
      current_state = State::PROVISION;
      render_state(current_state);
    }
  }
}

void app_tick() {
  lvgl_tick();
  switch (current_state) {
    case State::PROVISION:  perform_provision(); break;
    case State::DISCOVER:   perform_discover();  break;
    case State::PAIR:       perform_pair();      break;
    case State::POLL_RENDER:perform_poll();      break;
    default: break;
  }
}

} // namespace cyd
```

- [ ] **Step 3: Reduce main.cpp to a stub**

```cpp
// firmware/src/main.cpp
#include <Arduino.h>

#include "app/app_loop.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  cyd::app_init();
}

void loop() {
  cyd::app_tick();
  delay(5);
}
```

- [ ] **Step 4: Verify build**

Run: `cd firmware && pio run -e esp32dev`
Expected: build succeeds.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/app/app_loop.h firmware/src/app/app_loop.cpp firmware/src/main.cpp
git commit -m "wire state machine to UI and network I/O"
```

---

### Task 22: Long-press factory reset

**Files:**
- Create: `firmware/src/app/long_press.h`
- Create: `firmware/src/app/long_press.cpp`
- Modify: `firmware/src/app/app_loop.cpp`

- [ ] **Step 1: Declare a long-press detector**

```cpp
// firmware/src/app/long_press.h
#pragma once

#include <cstdint>

namespace cyd {

class LongPress {
 public:
  // Call every tick with current pressed state. Returns true exactly once
  // when the touch has been continuously pressed for hold_ms.
  bool update(bool pressed, uint32_t now_ms, uint32_t hold_ms);

 private:
  uint32_t pressed_since_ = 0;
  bool fired_ = false;
};

} // namespace cyd
```

- [ ] **Step 2: Implement**

```cpp
// firmware/src/app/long_press.cpp
#include "app/long_press.h"

namespace cyd {

bool LongPress::update(bool pressed, uint32_t now_ms, uint32_t hold_ms) {
  if (!pressed) {
    pressed_since_ = 0;
    fired_ = false;
    return false;
  }
  if (pressed_since_ == 0) {
    pressed_since_ = now_ms;
    return false;
  }
  if (!fired_ && now_ms - pressed_since_ >= hold_ms) {
    fired_ = true;
    return true;
  }
  return false;
}

} // namespace cyd
```

- [ ] **Step 3: Wire it into app_tick**

Add this to the top of `app_tick()` in `app_loop.cpp`, before the switch:

```cpp
static LongPress long_press;
auto ev = touch().poll();
if (long_press.update(ev.pressed, millis(), kLongPressMs)) {
  Serial.println("factory reset triggered");
  nvs_.factory_reset();
  ctx_ = Context{};
  pending_code_.clear();
  apply_event(Event::FACTORY_RESET);
}
```

And add the include at the top of `app_loop.cpp`:

```cpp
#include "app/long_press.h"
```

- [ ] **Step 4: Flash and verify**

Run: `pio run -e esp32dev -t upload && pio device monitor`

Expected: holding a finger anywhere on the screen for 5 s logs `factory reset triggered` and bounces the device back into PROVISION (Connect to WiFi screen).

- [ ] **Step 5: Commit**

```bash
git add firmware/src/app/long_press.h firmware/src/app/long_press.cpp firmware/src/app/app_loop.cpp
git commit -m "add 5s long-press factory reset"
```

---

### Task 23: End-to-end smoke verification

**Files:**
- Create: `firmware/scripts/e2e.sh`
- Modify: `firmware/README.md`

This is a documented manual procedure rather than an automated test — flashing a device cannot run in CI.

- [ ] **Step 1: Create the smoke script**

```bash
# firmware/scripts/e2e.sh
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
```

Make it executable:

```bash
chmod +x firmware/scripts/e2e.sh
```

- [ ] **Step 2: Update README**

Append to `firmware/README.md`:

```markdown
## End-to-end smoke

Spins up the daemon, flashes a connected CYD, and prints a manual checklist
covering provisioning, mDNS, pairing, and the offline recovery path.

```
./scripts/e2e.sh
```
```

- [ ] **Step 3: Run the smoke procedure on real hardware**

Run: `./firmware/scripts/e2e.sh`

Verify every checklist item passes. Note any deviations (e.g., pinout differences on your CYD revision) and either patch `firmware/src/hw/pins.h` or document the workaround in the README's troubleshooting section.

- [ ] **Step 4: Commit and merge**

```bash
git add firmware/scripts/e2e.sh firmware/README.md
git commit -m "add end-to-end smoke script and checklist"

git checkout main
git merge --no-ff feat/firmware-mvp -m "Merge feat/firmware-mvp: firmware MVP"
```

---

## Self-review checklist (engineer should re-run)

After all tasks, verify the spec is covered:

- [ ] Single binary, runtime touch detection (Task 6) — spec §Hardware Target
- [ ] PlatformIO + Arduino-ESP32 + LVGL 9.x + LovyanGFX (Tasks 1, 5, 7) — spec §Stack
- [ ] WiFiManager onboarding with custom title (Task 18) — spec §State machine
- [ ] mDNS discovery of `_claudeusage._tcp` (Task 19) — spec §State machine
- [ ] 4-digit pair code on touch confirm (Tasks 17, 20, 21) — spec §Pairing
- [ ] NVS persistence of WiFi creds, token, hostname, touch cal (Task 4) — spec §Persistence
- [ ] Six screens (Tasks 11–16) — spec §Screen structure
- [ ] Status bar + pip footer (Task 9) — spec §Screen structure
- [ ] 5 s active / 30 s idle polling with ±1 screen mask (Task 21) — spec §Polling
- [ ] Stale + backoff on daemon unreachable (Task 21) — spec §Error Handling
- [ ] 5 s long-press factory reset (Task 22) — spec §Persistence
- [ ] Schema-version check in parser (Task 2) — spec §Error Handling
- [ ] Empty-state on chat screen (Task 14) — out-of-scope guard for Phase 3
- [ ] Native unit tests for parser + state machine (Tasks 2, 3) — spec §Testing
