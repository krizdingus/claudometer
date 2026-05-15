// firmware/src/app/brightness_controller.cpp
#include "app/brightness_controller.h"

#include "app/app_config.h"

namespace cyd {

uint8_t curve(uint16_t adc) {
  // Lerp from (kAdcDark, kDutyFloor) to (kAdcBright, kDutyCeil), clamped.
  // Works for either LDR polarity: if kAdcDark > kAdcBright, the slope is
  // negative and the same code path produces the right answer.
  int32_t adc_span = (int32_t)kAdcBright - (int32_t)kAdcDark;
  if (adc_span == 0) return kDutyFloor;  // degenerate calibration

  int32_t adc_delta = (int32_t)adc - (int32_t)kAdcDark;
  // Compute t as Q8 fixed-point in [0, 256].
  int32_t t_q8 = (adc_delta * 256) / adc_span;
  if (t_q8 < 0)   t_q8 = 0;
  if (t_q8 > 256) t_q8 = 256;

  int32_t duty_span = (int32_t)kDutyCeil - (int32_t)kDutyFloor;
  int32_t duty = (int32_t)kDutyFloor + (t_q8 * duty_span) / 256;
  return (uint8_t)duty;
}

// ramp_step() defined in next steps.

}  // namespace cyd
