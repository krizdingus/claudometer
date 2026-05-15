// firmware/src/hw/light_sensor.cpp
#include "hw/light_sensor.h"

namespace cyd {

uint16_t ema_step(uint16_t prev, uint16_t raw) {
  // Single-pole IIR with α = 1/8. ~2 s time constant at 4 Hz sampling.
  return (uint16_t)(((uint32_t)prev * 7 + raw) / 8);
}

}  // namespace cyd
