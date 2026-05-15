#pragma once

#include <cstdint>

namespace cyd {

uint8_t curve(uint16_t adc);
uint8_t ramp_step(uint8_t current, uint8_t target, uint8_t max_step);

#ifndef UNIT_TEST

class LightSensor;
class Nvs;

class BrightnessController {
 public:
  void begin(LightSensor *sensor, Nvs *nvs);
  void tick(uint32_t now_ms);
  void set_auto(bool on);
  bool is_auto() const { return auto_mode_; }

 private:
  LightSensor *sensor_ = nullptr;
  Nvs *nvs_ = nullptr;
  bool auto_mode_ = true;
  uint8_t current_duty_ = 200;  // matches kDefaultBrightness
  uint32_t last_sample_ms_ = 0;
};

#endif  // UNIT_TEST

}  // namespace cyd
