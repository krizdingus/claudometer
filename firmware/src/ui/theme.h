#pragma once

#ifndef UNIT_TEST

#include <lvgl.h>

namespace cyd::theme {

constexpr uint32_t bg          = 0x0E0E10;
constexpr uint32_t fg          = 0xEDEDED;
constexpr uint32_t fg_muted    = 0x8A8A92;
constexpr uint32_t accent      = 0xD97757;   // Claude orange
constexpr uint32_t blue        = 0x6F9EFF;
constexpr uint32_t red         = 0xE85C5C;
constexpr uint32_t yellow      = 0xF5D24A;   // bezel chrome only
constexpr uint32_t green       = 0x7BD389;

inline lv_color_t c(uint32_t hex) { return lv_color_hex(hex); }

// Returns blue normally; red when pct >= 85.
lv_color_t bar_color_for_pct(int pct);

// Returns the appropriate pill background color for routine status.
lv_color_t status_pill_for(const char *status);

// Common style applied to all tile screens (bg + padding).
void apply_screen_styles(lv_obj_t *scr);

} // namespace cyd::theme

#endif  // UNIT_TEST
