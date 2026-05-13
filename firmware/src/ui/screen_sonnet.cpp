#include "ui/screen_sonnet.h"

#ifndef UNIT_TEST

#include <stdio.h>
#include "ui/theme.h"

namespace cyd {

void ScreenSonnet::build(lv_obj_t *parent) {
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Sonnet - This Week");
  lv_obj_set_style_text_color(title, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

  pct_ = lv_label_create(parent);
  lv_obj_set_style_text_color(pct_, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(pct_, &lv_font_montserrat_32, 0);
  lv_obj_align(pct_, LV_ALIGN_TOP_LEFT, 4, 24);

  bar_ = lv_bar_create(parent);
  lv_obj_set_size(bar_, 220, 18);
  lv_obj_align(bar_, LV_ALIGN_TOP_LEFT, 4, 80);
  lv_bar_set_range(bar_, 0, 100);
  lv_obj_set_style_bg_color(bar_, theme::c(0x222226), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar_, theme::c(theme::blue), LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar_, 8, LV_PART_MAIN);
  lv_obj_set_style_radius(bar_, 8, LV_PART_INDICATOR);

  amount_ = lv_label_create(parent);
  lv_obj_set_style_text_color(amount_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(amount_, &lv_font_montserrat_14, 0);
  lv_obj_align(amount_, LV_ALIGN_TOP_LEFT, 4, 110);

  pace_ = lv_label_create(parent);
  lv_obj_set_style_text_color(pace_, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(pace_, &lv_font_montserrat_16, 0);
  lv_obj_align(pace_, LV_ALIGN_TOP_LEFT, 4, 150);
}

void ScreenSonnet::update(const Stats &s) {
  char buf[40];
  snprintf(buf, sizeof(buf), "%d%%", s.sonnet.weekly_pct);
  lv_label_set_text(pct_, buf);
  lv_bar_set_value(bar_, s.sonnet.weekly_pct, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar_, theme::bar_color_for_pct(s.sonnet.weekly_pct),
                            LV_PART_INDICATOR);
  snprintf(buf, sizeof(buf), "%dk / %dk tokens",
           s.sonnet.used / 1000, s.sonnet.cap / 1000);
  lv_label_set_text(amount_, buf);

  const char *pace = "-";
  uint32_t color = theme::fg_muted;
  if (s.sonnet.pace == "ahead")         { pace = "ahead of pace";  color = theme::yellow; }
  else if (s.sonnet.pace == "on_track") { pace = "on pace";        color = theme::green;  }
  else if (s.sonnet.pace == "behind")   { pace = "behind pace";    color = theme::blue;   }
  lv_label_set_text(pace_, pace);
  lv_obj_set_style_text_color(pace_, theme::c(color), 0);
}

} // namespace cyd

#endif  // UNIT_TEST
