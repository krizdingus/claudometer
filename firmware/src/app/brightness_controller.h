#pragma once

#include <cstdint>

namespace cyd {

#ifndef UNIT_TEST

class Nvs;

class BrightnessController {
 public:
  void begin(Nvs *nvs);
  void set_level(uint8_t duty);  // applies via display() + persists in NVS
  uint8_t level() const { return current_duty_; }

 private:
  Nvs *nvs_ = nullptr;
  uint8_t current_duty_ = 200;  // matches kBrightnessDefault
};

#endif  // UNIT_TEST

}  // namespace cyd
