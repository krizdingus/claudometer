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
