#pragma once

#ifndef UNIT_TEST

#include <lvgl.h>

namespace cyd::theme {

enum class Mode { Dark, Light };

struct Palette {
  uint32_t bg;
  uint32_t fg;
  uint32_t fg_muted;
  uint32_t accent;
  uint32_t bar_bg;
  uint32_t ok;
  uint32_t warn;
  uint32_t alert;
};

void set_mode(Mode m);
Mode get_mode();
const Palette& palette();

lv_color_t bg();
lv_color_t fg();
lv_color_t fg_muted();
lv_color_t accent();
lv_color_t bar_bg();
lv_color_t ok();
lv_color_t warn();
lv_color_t alert();

// Returns accent normally; alert when pct >= 85.
lv_color_t bar_color_for_pct(int pct);

// Routine status pill color: ok/slow/fail/queued.
lv_color_t status_pill_for(const char *status);

// Common style applied to all tile screens (bg + padding).
void apply_screen_styles(lv_obj_t *scr);

// Returns lv_color_t from a raw hex value. Kept for one-off colors that aren't
// in the palette (e.g., subtle backgrounds drawn directly).
inline lv_color_t hex(uint32_t v) { return lv_color_hex(v); }

}  // namespace cyd::theme

#endif  // UNIT_TEST
