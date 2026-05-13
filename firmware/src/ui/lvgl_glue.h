#pragma once

namespace cyd {

// Initialise LVGL, register display + input devices, allocate draw buffers.
// Must be called once after display() and touch() are initialised.
void lvgl_init();

// Drive LVGL's timers + flush queue. Call from loop().
void lvgl_tick();

} // namespace cyd
