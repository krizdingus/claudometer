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
