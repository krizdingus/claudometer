#pragma once

namespace cyd {

// Initialises display/touch/lvgl, builds the persistent chrome, loads NVS,
// and drops into the state machine. Call once from setup().
void app_init();

// Drive LVGL, run state-machine ticks, kick off network I/O on cadence.
// Call from loop().
void app_tick();

} // namespace cyd
