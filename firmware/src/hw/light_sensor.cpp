// firmware/src/hw/light_sensor.cpp
#include "hw/light_sensor.h"

#ifndef UNIT_TEST
#include <Arduino.h>
#include "hw/pins.h"
#endif

namespace cyd {

uint16_t ema_step(uint16_t prev, uint16_t raw) {
  // Single-pole IIR with α = 1/8. ~2 s time constant at 4 Hz sampling.
  return (uint16_t)(((uint32_t)prev * 7 + raw) / 8);
}

#ifndef UNIT_TEST

void LightSensor::begin() {
  analogSetPinAttenuation(pins::LDR, ADC_11db);  // ~0–3.3 V range
  smoothed_ = analogRead(pins::LDR);             // prime EMA with one real read
}

uint16_t LightSensor::read_smoothed() {
  uint16_t raw = analogRead(pins::LDR);
  smoothed_ = ema_step(smoothed_, raw);
  return smoothed_;
}

#endif  // UNIT_TEST

}  // namespace cyd
