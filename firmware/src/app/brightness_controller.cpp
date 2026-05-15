#include "app/brightness_controller.h"

#include "app/app_config.h"

#ifndef UNIT_TEST
#include <Arduino.h>
#include "hw/display.h"
#include "hw/nvs.h"
#endif

namespace cyd {

#ifndef UNIT_TEST

void BrightnessController::begin(Nvs *nvs) {
  nvs_ = nvs;
  current_duty_ = nvs_->brightness();
  // display().init() must have run first — this writes via setBrightness().
  display().setBrightness(current_duty_);
}

void BrightnessController::set_level(uint8_t duty) {
  current_duty_ = duty;
  if (nvs_) nvs_->save_brightness(duty);
  display().setBrightness(duty);
  Serial.printf("brightness: level=%u\n", duty);
}

#endif  // UNIT_TEST

}  // namespace cyd
