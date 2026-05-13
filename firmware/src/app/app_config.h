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
  SCR_MODELS,
  SCR_SONNET,
  SCR_ROUTINES,
  SCR_BUDGETS,
  SCR_COUNT,
};

constexpr int kStatusBarHeight = 18;
constexpr int kFooterHeight    = 18;

} // namespace cyd
