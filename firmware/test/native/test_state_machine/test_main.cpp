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
