#include "ui/chrome.h"

#ifndef UNIT_TEST

#include "ui/theme.h"

namespace cyd {

void Chrome::attach(lv_obj_t *parent) {
  // Status bar
  auto *bar = lv_obj_create(parent);
  lv_obj_set_size(bar, 240, kStatusBarHeight);
  lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(bar, theme::c(theme::bg), 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_pad_all(bar, 2, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  dot_ = lv_obj_create(bar);
  lv_obj_set_size(dot_, 8, 8);
  lv_obj_align(dot_, LV_ALIGN_LEFT_MID, 4, 0);
  lv_obj_set_style_radius(dot_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(dot_, 0, 0);
  lv_obj_set_style_bg_color(dot_, theme::c(theme::green), 0);

  auto *dot_label = lv_label_create(bar);
  lv_obj_align(dot_label, LV_ALIGN_LEFT_MID, 16, 0);
  lv_obj_set_style_text_color(dot_label, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(dot_label, &lv_font_montserrat_12, 0);
  lv_label_set_text(dot_label, "daemon");

  clock_ = lv_label_create(bar);
  lv_obj_align(clock_, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_set_style_text_color(clock_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(clock_, &lv_font_montserrat_12, 0);
  lv_label_set_text(clock_, "--:--");

  // Pip footer
  auto *foot = lv_obj_create(parent);
  lv_obj_set_size(foot, 240, kFooterHeight);
  lv_obj_align(foot, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(foot, theme::c(theme::bg), 0);
  lv_obj_set_style_border_width(foot, 0, 0);
  lv_obj_set_style_pad_all(foot, 2, 0);
  lv_obj_clear_flag(foot, LV_OBJ_FLAG_SCROLLABLE);

  constexpr int kPipSize = 6;
  constexpr int kPipGap  = 10;
  int total = SCR_COUNT * kPipSize + (SCR_COUNT - 1) * kPipGap;
  int x0 = (240 - total) / 2;
  for (int i = 0; i < SCR_COUNT; ++i) {
    pips_[i] = lv_obj_create(foot);
    lv_obj_set_size(pips_[i], kPipSize, kPipSize);
    lv_obj_set_pos(pips_[i], x0 + i * (kPipSize + kPipGap), 4);
    lv_obj_set_style_radius(pips_[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(pips_[i], 0, 0);
    lv_obj_set_style_bg_color(pips_[i], theme::c(theme::fg_muted), 0);
  }
  set_active_screen(0);
}

void Chrome::set_health(int health) {
  uint32_t color = theme::green;
  if (health == 1) color = theme::yellow;
  if (health == 2) color = theme::red;
  lv_obj_set_style_bg_color(dot_, theme::c(color), 0);
}

void Chrome::set_clock(const char *hhmm) {
  lv_label_set_text(clock_, hhmm ? hhmm : "--:--");
}

void Chrome::set_active_screen(int index) {
  for (int i = 0; i < SCR_COUNT; ++i) {
    bool on = (i == index);
    lv_obj_set_style_bg_color(pips_[i],
                              theme::c(on ? theme::accent : theme::fg_muted), 0);
  }
}

} // namespace cyd

#endif  // UNIT_TEST
