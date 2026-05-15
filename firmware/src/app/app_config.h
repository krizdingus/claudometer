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
  SCR_HOME = 0,
  SCR_SESSION,
  SCR_BUDGETS,
  SCR_MODELS,
  SCR_ROUTINES,
  SCR_SETTINGS,
  SCR_COUNT,
};

constexpr int kStatusBarHeight = 18;
constexpr int kFooterHeight    = 18;

// Auto-brightness. See spec 2026-05-15-auto-brightness-design.md.
// Curve endpoints are CALIBRATED ON HARDWARE — see plan Task 12.
constexpr uint32_t kSamplePeriodMs   = 250;   // 4 Hz sampling.
constexpr uint8_t  kMaxStepPerTick   = 8;     // Ramp slew; ~8 s for full swing.
constexpr uint8_t  kDefaultBrightness = 200;  // Used when Auto is Off.
constexpr uint16_t kAdcDark   = 200;          // PLACEHOLDER — calibrate.
constexpr uint16_t kAdcBright = 3000;         // PLACEHOLDER — calibrate.
constexpr uint8_t  kDutyFloor = 40;           // ~16% of 255.
constexpr uint8_t  kDutyCeil  = 255;

} // namespace cyd
