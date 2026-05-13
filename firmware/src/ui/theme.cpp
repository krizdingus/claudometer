#include "ui/theme.h"

#ifndef UNIT_TEST

#include <string.h>

namespace cyd::theme {

lv_color_t bar_color_for_pct(int pct) {
  return pct >= 85 ? c(red) : c(blue);
}

lv_color_t status_pill_for(const char *status) {
  if (!status) return c(fg_muted);
  if (strcmp(status, "ok") == 0)     return c(green);
  if (strcmp(status, "slow") == 0)   return c(yellow);
  if (strcmp(status, "fail") == 0)   return c(red);
  if (strcmp(status, "queued") == 0) return c(blue);
  return c(fg_muted);
}

void apply_screen_styles(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, c(bg), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(scr, 8, 0);
  lv_obj_set_style_text_color(scr, c(fg), 0);
  lv_obj_set_style_text_font(scr, &lv_font_montserrat_14, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
}

} // namespace cyd::theme

#endif  // UNIT_TEST
