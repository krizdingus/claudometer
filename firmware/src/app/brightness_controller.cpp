#include "app/brightness_controller.h"

#include "app/app_config.h"

#ifndef UNIT_TEST
#include <Arduino.h>
#include "hw/display.h"
#include "hw/light_sensor.h"
#include "hw/nvs.h"
#endif

namespace cyd {

uint8_t curve(uint16_t adc) {
  int32_t adc_span = (int32_t)kAdcBright - (int32_t)kAdcDark;
  if (adc_span == 0) return kDutyFloor;
  int32_t adc_delta = (int32_t)adc - (int32_t)kAdcDark;
  int32_t t_q8 = (adc_delta * 256) / adc_span;
  if (t_q8 < 0)   t_q8 = 0;
  if (t_q8 > 256) t_q8 = 256;
  int32_t duty_span = (int32_t)kDutyCeil - (int32_t)kDutyFloor;
  int32_t duty = (int32_t)kDutyFloor + (t_q8 * duty_span) / 256;
  return (uint8_t)duty;
}

uint8_t ramp_step(uint8_t current, uint8_t target, uint8_t max_step) {
  if (current == target) return current;
  if (current < target) {
    uint8_t delta = target - current;
    return delta <= max_step ? target : (uint8_t)(current + max_step);
  } else {
    uint8_t delta = current - target;
    return delta <= max_step ? target : (uint8_t)(current - max_step);
  }
}

#ifndef UNIT_TEST

void BrightnessController::begin(LightSensor *sensor, Nvs *nvs) {
  sensor_ = sensor;
  nvs_ = nvs;
  auto_mode_ = nvs_->auto_bright();  // defaults to true if unset (see nvs.cpp)
  current_duty_ = kDefaultBrightness;
  // Caller must have already called display().init() and setBrightness() at
  // boot — this write sets us up at the same value so the controller's first
  // ramp starts from a known place.
  display().setBrightness(current_duty_);
  last_sample_ms_ = 0;  // first tick samples immediately after kSamplePeriodMs
}

void BrightnessController::tick(uint32_t now_ms) {
  if (!auto_mode_) return;
  if (now_ms - last_sample_ms_ < kSamplePeriodMs) return;
  last_sample_ms_ = now_ms;

  uint16_t smoothed = sensor_->read_smoothed();
  uint8_t target = curve(smoothed);
  current_duty_ = ramp_step(current_duty_, target, kMaxStepPerTick);
  display().setBrightness(current_duty_);
}

void BrightnessController::set_auto(bool on) {
  auto_mode_ = on;
  if (nvs_) nvs_->save_auto_bright(on);
  Serial.printf("brightness: auto=%s\n", on ? "on" : "off");
  if (on) {
    last_sample_ms_ = 0;  // sample on next tick
  } else {
    current_duty_ = kDefaultBrightness;
    display().setBrightness(current_duty_);
  }
}

#endif  // UNIT_TEST

}  // namespace cyd
