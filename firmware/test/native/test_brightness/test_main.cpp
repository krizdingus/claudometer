#include <unity.h>

#include "app/app_config.h"
#include "app/brightness_controller.h"
#include "hw/light_sensor.h"

void test_curve_at_dark_endpoint_returns_floor(void) {
  TEST_ASSERT_EQUAL_UINT8(cyd::kDutyFloor, cyd::curve(cyd::kAdcDark));
}

void test_curve_at_bright_endpoint_returns_ceil(void) {
  TEST_ASSERT_EQUAL_UINT8(cyd::kDutyCeil, cyd::curve(cyd::kAdcBright));
}

void test_curve_below_dark_clamps_to_floor(void) {
  TEST_ASSERT_EQUAL_UINT8(cyd::kDutyFloor, cyd::curve(0));
}

void test_curve_above_bright_clamps_to_ceil(void) {
  TEST_ASSERT_EQUAL_UINT8(cyd::kDutyCeil, cyd::curve(4095));
}

void test_curve_midpoint_is_roughly_midpoint_duty(void) {
  uint16_t mid_adc = (cyd::kAdcDark + cyd::kAdcBright) / 2;
  uint8_t  mid_duty = (cyd::kDutyFloor + cyd::kDutyCeil) / 2;
  uint8_t  got = cyd::curve(mid_adc);
  // Allow ±2 LSB rounding.
  TEST_ASSERT_INT_WITHIN(2, mid_duty, got);
}

void test_ramp_step_no_move_when_at_target(void) {
  TEST_ASSERT_EQUAL_UINT8(100, cyd::ramp_step(100, 100, 8));
}

void test_ramp_step_lands_on_target_when_within_step(void) {
  TEST_ASSERT_EQUAL_UINT8(105, cyd::ramp_step(100, 105, 8));  // delta 5 ≤ 8 → snap
  TEST_ASSERT_EQUAL_UINT8(95,  cyd::ramp_step(100, 95,  8));  // delta 5 ≤ 8 → snap
}

void test_ramp_step_moves_by_max_when_far(void) {
  TEST_ASSERT_EQUAL_UINT8(108, cyd::ramp_step(100, 200, 8));  // moves +max
  TEST_ASSERT_EQUAL_UINT8(92,  cyd::ramp_step(100, 0,   8));  // moves -max
}

void test_ramp_step_lands_exactly_on_boundary(void) {
  TEST_ASSERT_EQUAL_UINT8(108, cyd::ramp_step(100, 108, 8));  // delta == max → snap
}

void test_ema_step_converges_toward_target(void) {
  // Integer IIR saturates near target due to truncation; allow ±10 LSB.
  uint16_t smoothed = 0;
  for (int i = 0; i < 50; i++) {
    smoothed = cyd::ema_step(smoothed, 1000);
  }
  TEST_ASSERT_INT_WITHIN(10, 1000, smoothed);
}

void test_ema_step_no_change_at_target(void) {
  TEST_ASSERT_EQUAL_UINT16(500, cyd::ema_step(500, 500));
}

void test_ema_step_moves_toward_lower_value(void) {
  // From 800 toward 0: (800*7 + 0)/8 = 700.
  TEST_ASSERT_EQUAL_UINT16(700, cyd::ema_step(800, 0));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_curve_at_dark_endpoint_returns_floor);
  RUN_TEST(test_curve_at_bright_endpoint_returns_ceil);
  RUN_TEST(test_curve_below_dark_clamps_to_floor);
  RUN_TEST(test_curve_above_bright_clamps_to_ceil);
  RUN_TEST(test_curve_midpoint_is_roughly_midpoint_duty);
  RUN_TEST(test_ramp_step_no_move_when_at_target);
  RUN_TEST(test_ramp_step_lands_on_target_when_within_step);
  RUN_TEST(test_ramp_step_moves_by_max_when_far);
  RUN_TEST(test_ramp_step_lands_exactly_on_boundary);
  RUN_TEST(test_ema_step_converges_toward_target);
  RUN_TEST(test_ema_step_no_change_at_target);
  RUN_TEST(test_ema_step_moves_toward_lower_value);
  return UNITY_END();
}
