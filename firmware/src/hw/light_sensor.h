// firmware/src/hw/light_sensor.h
#pragma once

#include <cstdint>

namespace cyd {

uint16_t ema_step(uint16_t prev, uint16_t raw);

#ifndef UNIT_TEST

class LightSensor {
 public:
  void begin();
  uint16_t read_smoothed();

 private:
  uint16_t smoothed_ = 0;
};

#endif  // UNIT_TEST

}  // namespace cyd
