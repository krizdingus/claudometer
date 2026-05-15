#include "ui/theme.h"

#ifndef UNIT_TEST

#include <string.h>

namespace cyd::theme {

namespace {

constexpr Palette kDark = {
    .bg       = 0x16140F,
    .fg       = 0xF0EEE6,
    .fg_muted = 0x8A857B,
    .accent   = 0xD97757,
    .bar_bg   = 0x26241D,
    .ok       = 0x7BB58A,
    .warn     = 0xE8C46B,
    .alert    = 0xD9614D,
};

constexpr Palette kLight = {
    .bg       = 0xF0EEE6,
    .fg       = 0x1F1E1B,
    .fg_muted = 0x7A766E,
    .accent   = 0xC26542,
    .bar_bg   = 0xE1DED2,
    .ok       = 0x5E9A6E,
    .warn     = 0xC9A23F,
    .alert    = 0xB85240,
};

Mode current_ = Mode::Dark;

}  // namespace

void set_mode(Mode m) { current_ = m; }
Mode get_mode() { return current_; }

const Palette& palette() {
  return current_ == Mode::Light ? kLight : kDark;
}

lv_color_t bg()       { return lv_color_hex(palette().bg); }
lv_color_t fg()       { return lv_color_hex(palette().fg); }
lv_color_t fg_muted() { return lv_color_hex(palette().fg_muted); }
lv_color_t accent()   { return lv_color_hex(palette().accent); }
lv_color_t bar_bg()   { return lv_color_hex(palette().bar_bg); }
lv_color_t ok()       { return lv_color_hex(palette().ok); }
lv_color_t warn()     { return lv_color_hex(palette().warn); }
lv_color_t alert()    { return lv_color_hex(palette().alert); }

lv_color_t bar_color_for_pct(int pct) {
  return pct >= 85 ? alert() : accent();
}

lv_color_t status_pill_for(const char *status) {
  if (!status) return fg_muted();
  if (strcmp(status, "ok") == 0)     return ok();
  if (strcmp(status, "slow") == 0)   return warn();
  if (strcmp(status, "fail") == 0)   return alert();
  if (strcmp(status, "queued") == 0) return accent();
  return fg_muted();
}

void apply_screen_styles(lv_obj_t *scr) {
  lv_obj_set_size(scr, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(scr, bg(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(scr, 0, 0);
  lv_obj_set_style_radius(scr, 0, 0);
  lv_obj_set_style_pad_all(scr, 8, 0);
  lv_obj_set_style_text_color(scr, fg(), 0);
  lv_obj_set_style_text_font(scr, &lv_font_montserrat_14, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
}

}  // namespace cyd::theme

#endif  // UNIT_TEST
