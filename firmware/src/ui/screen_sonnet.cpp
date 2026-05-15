#include "ui/screen_sonnet.h"

#ifndef UNIT_TEST

#include "ui/theme.h"

namespace cyd {

void ScreenSonnet::build(lv_obj_t *parent) {
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Sonnet - This Week");
  lv_obj_set_style_text_color(title, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

  pct_ = lv_label_create(parent);
  lv_obj_set_style_text_color(pct_, theme::fg(), 0);
  lv_obj_set_style_text_font(pct_, &lv_font_montserrat_32, 0);
  lv_obj_align(pct_, LV_ALIGN_TOP_LEFT, 4, 24);

  bar_ = lv_bar_create(parent);
  lv_obj_set_size(bar_, 220, 18);
  lv_obj_align(bar_, LV_ALIGN_TOP_LEFT, 4, 80);
  lv_bar_set_range(bar_, 0, 100);
  lv_obj_set_style_bg_color(bar_, theme::bar_bg(), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar_, theme::accent(), LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar_, 8, LV_PART_MAIN);
  lv_obj_set_style_radius(bar_, 8, LV_PART_INDICATOR);

  amount_ = lv_label_create(parent);
  lv_obj_set_style_text_color(amount_, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(amount_, &lv_font_montserrat_14, 0);
  lv_obj_align(amount_, LV_ALIGN_TOP_LEFT, 4, 110);

  pace_ = lv_label_create(parent);
  lv_obj_set_style_text_color(pace_, theme::fg(), 0);
  lv_obj_set_style_text_font(pace_, &lv_font_montserrat_16, 0);
  lv_obj_align(pace_, LV_ALIGN_TOP_LEFT, 4, 150);
}

void ScreenSonnet::update(const Stats &) {
  // Sonnet stat removed from Stats in Task 2; screen deleted in Task 8.
  // Body intentionally empty.
}

} // namespace cyd

#endif  // UNIT_TEST
