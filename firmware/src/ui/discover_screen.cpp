#include "ui/discover_screen.h"

#ifndef UNIT_TEST

#include "ui/theme.h"

namespace cyd {

void DiscoverScreen::build(lv_obj_t *parent) {
  theme::apply_screen_styles(parent);
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Looking for daemon");
  lv_obj_set_style_text_color(title, theme::c(theme::accent), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 60);

  status_ = lv_label_create(parent);
  lv_label_set_long_mode(status_, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(status_, 220);
  lv_obj_set_style_text_color(status_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(status_, &lv_font_montserrat_14, 0);
  lv_obj_align(status_, LV_ALIGN_TOP_MID, 0, 130);
  lv_label_set_text(status_,
      "Make sure the daemon is\nrunning on your Mac:\n\n  brew install cydmonitor");
}

void DiscoverScreen::set_status(const char *s) {
  if (s) lv_label_set_text(status_, s);
}

} // namespace cyd

#endif  // UNIT_TEST
