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
