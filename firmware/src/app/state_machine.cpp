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
