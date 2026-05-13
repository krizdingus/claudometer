#include "ui/screen_chat.h"

#ifndef UNIT_TEST

#include <stdio.h>
#include "ui/theme.h"

namespace cyd {

void ScreenChat::build(lv_obj_t *parent) {
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Claude.ai · Chat");
  lv_obj_set_style_text_color(title, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

  count_ = lv_label_create(parent);
  lv_obj_set_style_text_color(count_, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(count_, &lv_font_montserrat_32, 0);
  lv_obj_align(count_, LV_ALIGN_TOP_LEFT, 4, 30);
  lv_label_set_text(count_, "—");

  cap_ = lv_label_create(parent);
  lv_obj_set_style_text_color(cap_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(cap_, &lv_font_montserrat_14, 0);
  lv_obj_align(cap_, LV_ALIGN_TOP_LEFT, 4, 80);
  lv_label_set_text(cap_, "");

  bar_ = lv_bar_create(parent);
  lv_obj_set_size(bar_, 220, 14);
  lv_obj_align(bar_, LV_ALIGN_TOP_LEFT, 4, 110);
  lv_bar_set_range(bar_, 0, 100);
  lv_obj_set_style_bg_color(bar_, theme::c(0x222226), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar_, theme::c(theme::blue), LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar_, 6, LV_PART_MAIN);
  lv_obj_set_style_radius(bar_, 6, LV_PART_INDICATOR);

  empty_ = lv_label_create(parent);
  lv_obj_set_style_text_color(empty_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(empty_, &lv_font_montserrat_14, 0);
  lv_label_set_long_mode(empty_, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(empty_, 220);
  lv_obj_align(empty_, LV_ALIGN_TOP_LEFT, 4, 150);
  lv_label_set_text(empty_, "Sign in to Claude.ai\nfrom localhost:7842 to\ntrack daily chat.");
}

void ScreenChat::update(const Stats &s) {
  bool empty = (s.chat.daily_cap == 0 && s.chat.messages_today == 0);
  if (empty) {
    lv_obj_clear_flag(empty_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(bar_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(cap_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(count_, "—");
    return;
  }
  lv_obj_add_flag(empty_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(bar_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(cap_, LV_OBJ_FLAG_HIDDEN);

  char buf[24];
  snprintf(buf, sizeof(buf), "%d", s.chat.messages_today);
  lv_label_set_text(count_, buf);
  snprintf(buf, sizeof(buf), "of %d daily", s.chat.daily_cap);
  lv_label_set_text(cap_, buf);
  int pct = s.chat.daily_cap > 0
                ? (s.chat.messages_today * 100) / s.chat.daily_cap : 0;
  lv_bar_set_value(bar_, pct, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar_, theme::bar_color_for_pct(pct), LV_PART_INDICATOR);
}

} // namespace cyd

#endif  // UNIT_TEST
