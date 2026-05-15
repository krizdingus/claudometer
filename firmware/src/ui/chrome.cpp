#include "ui/chrome.h"

#ifndef UNIT_TEST

#include "ui/theme.h"

namespace cyd {

void Chrome::attach(lv_obj_t *parent) {
  // Status bar
  auto *bar = lv_obj_create(parent);
  lv_obj_set_size(bar, 240, kStatusBarHeight);
  lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(bar, theme::bg(), 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_pad_all(bar, 2, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  dot_ = lv_obj_create(bar);
  lv_obj_set_size(dot_, 8, 8);
  lv_obj_align(dot_, LV_ALIGN_LEFT_MID, 4, 0);
  lv_obj_set_style_radius(dot_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(dot_, 0, 0);
  lv_obj_set_style_bg_color(dot_, theme::ok(), 0);

  plan_pill_ = lv_obj_create(bar);
  lv_obj_set_size(plan_pill_, 70, 14);
  lv_obj_align(plan_pill_, LV_ALIGN_LEFT_MID, 18, 0);
  lv_obj_set_style_bg_color(plan_pill_, theme::accent(), 0);
  lv_obj_set_style_bg_opa(plan_pill_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(plan_pill_, 0, 0);
  lv_obj_set_style_radius(plan_pill_, 7, 0);
  lv_obj_set_style_pad_all(plan_pill_, 0, 0);
  lv_obj_clear_flag(plan_pill_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(plan_pill_, LV_OBJ_FLAG_HIDDEN);

  plan_pill_label_ = lv_label_create(plan_pill_);
  lv_obj_set_style_text_color(plan_pill_label_, theme::bg(), 0);
  lv_obj_set_style_text_font(plan_pill_label_, &lv_font_montserrat_12, 0);
  lv_label_set_text(plan_pill_label_, "");
  lv_obj_center(plan_pill_label_);

  clock_ = lv_label_create(bar);
  lv_obj_align(clock_, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_set_style_text_color(clock_, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(clock_, &lv_font_montserrat_12, 0);
  lv_label_set_text(clock_, "--:--");

  // Pip footer
  auto *foot = lv_obj_create(parent);
  lv_obj_set_size(foot, 240, kFooterHeight);
  lv_obj_align(foot, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(foot, theme::bg(), 0);
  lv_obj_set_style_border_width(foot, 0, 0);
  lv_obj_set_style_pad_all(foot, 2, 0);
  lv_obj_clear_flag(foot, LV_OBJ_FLAG_SCROLLABLE);

  constexpr int kPipSize = 4;
  constexpr int kPipGap  = 12;
  int total = SCR_COUNT * kPipSize + (SCR_COUNT - 1) * kPipGap;
  int x0 = (240 - total) / 2;
  for (int i = 0; i < SCR_COUNT; ++i) {
    pips_[i] = lv_obj_create(foot);
    lv_obj_set_size(pips_[i], kPipSize, kPipSize);
    lv_obj_set_pos(pips_[i], x0 + i * (kPipSize + kPipGap), 6);
    lv_obj_set_style_radius(pips_[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(pips_[i], 0, 0);
    lv_obj_set_style_bg_color(pips_[i], theme::fg_muted(), 0);
  }
  set_active_screen(0);
}

void Chrome::set_health(int health) {
  lv_color_t color = theme::ok();
  if (health == 1) color = theme::warn();
  if (health == 2) color = theme::alert();
  lv_obj_set_style_bg_color(dot_, color, 0);
}

void Chrome::set_clock(const char *hhmm) {
  lv_label_set_text(clock_, hhmm ? hhmm : "--:--");
}

void Chrome::set_active_screen(int index) {
  for (int i = 0; i < SCR_COUNT; ++i) {
    bool on = (i == index);
    lv_obj_set_style_bg_color(pips_[i], on ? theme::accent() : theme::fg_muted(), 0);
  }
}

void Chrome::set_plan(const char *pretty_plan) {
  if (!pretty_plan || pretty_plan[0] == '\0' || pretty_plan[0] == '-') {
    lv_obj_add_flag(plan_pill_, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_label_set_text(plan_pill_label_, pretty_plan);
  lv_obj_clear_flag(plan_pill_, LV_OBJ_FLAG_HIDDEN);
}

} // namespace cyd

#endif  // UNIT_TEST
