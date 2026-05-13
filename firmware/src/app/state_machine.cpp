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
