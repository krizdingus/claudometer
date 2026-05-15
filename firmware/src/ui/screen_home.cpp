#include "ui/screen_home.h"

#ifndef UNIT_TEST

#include <stdio.h>
#include <string.h>
#include <algorithm>

#include "ui/theme.h"

namespace cyd {

namespace {

lv_obj_t *hairline(lv_obj_t *parent, int y) {
  auto *h = lv_obj_create(parent);
  lv_obj_set_size(h, 224, 1);
  lv_obj_align(h, LV_ALIGN_TOP_LEFT, 4, y);
  lv_obj_set_style_bg_color(h, theme::bar_bg(), 0);
  lv_obj_set_style_bg_opa(h, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(h, 0, 0);
  lv_obj_set_style_radius(h, 0, 0);
  return h;
}

lv_obj_t *make_bar(lv_obj_t *parent, int y, int width, int height) {
  auto *b = lv_bar_create(parent);
  lv_obj_set_size(b, width, height);
  lv_obj_align(b, LV_ALIGN_TOP_LEFT, 4, y);
  lv_bar_set_range(b, 0, 100);
  lv_obj_set_style_bg_color(b, theme::bar_bg(), LV_PART_MAIN);
  lv_obj_set_style_bg_color(b, theme::accent(), LV_PART_INDICATOR);
  lv_obj_set_style_radius(b, height / 2, LV_PART_MAIN);
  lv_obj_set_style_radius(b, height / 2, LV_PART_INDICATOR);
  return b;
}

void set_bar(lv_obj_t *bar, int pct) {
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  lv_bar_set_value(bar, pct, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar, theme::bar_color_for_pct(pct), LV_PART_INDICATOR);
}

}  // namespace

void ScreenHome::build(lv_obj_t *parent) {
  // SESSION block (hero)
  auto *session_label = lv_label_create(parent);
  lv_label_set_text(session_label, "SESSION");
  lv_obj_set_style_text_color(session_label, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(session_label, &lv_font_montserrat_12, 0);
  lv_obj_align(session_label, LV_ALIGN_TOP_LEFT, 4, 4);

  session_pct_ = lv_label_create(parent);
  lv_obj_set_style_text_color(session_pct_, theme::fg(), 0);
  lv_obj_set_style_text_font(session_pct_, &lv_font_montserrat_48, 0);
  lv_obj_align(session_pct_, LV_ALIGN_TOP_LEFT, 4, 20);
  lv_label_set_text(session_pct_, "--%");

  session_bar_ = make_bar(parent, 80, 224, 8);

  session_meta_ = lv_label_create(parent);
  lv_obj_set_style_text_color(session_meta_, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(session_meta_, &lv_font_montserrat_12, 0);
  lv_obj_align(session_meta_, LV_ALIGN_TOP_LEFT, 4, 96);
  lv_label_set_text(session_meta_, "no active session");

  hairline(parent, 118);

  // WEEK block (secondary)
  auto *week_label = lv_label_create(parent);
  lv_label_set_text(week_label, "WEEK");
  lv_obj_set_style_text_color(week_label, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(week_label, &lv_font_montserrat_12, 0);
  lv_obj_align(week_label, LV_ALIGN_TOP_LEFT, 4, 128);

  week_pct_ = lv_label_create(parent);
  lv_obj_set_style_text_color(week_pct_, theme::fg(), 0);
  lv_obj_set_style_text_font(week_pct_, &lv_font_montserrat_24, 0);
  lv_obj_align(week_pct_, LV_ALIGN_TOP_RIGHT, -4, 124);
  lv_label_set_text(week_pct_, "--%");

  week_bar_ = make_bar(parent, 154, 224, 6);

  week_meta_ = lv_label_create(parent);
  lv_obj_set_style_text_color(week_meta_, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(week_meta_, &lv_font_montserrat_12, 0);
  lv_obj_align(week_meta_, LV_ALIGN_TOP_LEFT, 4, 166);
  lv_label_set_text(week_meta_, "-");

  hairline(parent, 188);

  // TODAY block (tertiary)
  auto *today_label = lv_label_create(parent);
  lv_label_set_text(today_label, "TODAY");
  lv_obj_set_style_text_color(today_label, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(today_label, &lv_font_montserrat_12, 0);
  lv_obj_align(today_label, LV_ALIGN_TOP_LEFT, 4, 198);

  today_line_ = lv_label_create(parent);
  lv_obj_set_style_text_color(today_line_, theme::fg(), 0);
  lv_obj_set_style_text_font(today_line_, &lv_font_montserrat_14, 0);
  lv_obj_align(today_line_, LV_ALIGN_TOP_RIGHT, -4, 198);
  lv_label_set_text(today_line_, "0k - $0.00");
}

void ScreenHome::update(const Stats &s) {
  // Plan pill is owned by chrome — updated from app_loop.cpp.

  int sp = s.session.pct_used;
  char buf[64];
  snprintf(buf, sizeof(buf), "%d%%", sp);
  lv_label_set_text(session_pct_, buf);
  set_bar(session_bar_, sp);

  if (!s.session.resets_at.empty()) {
    int m = s.session.minutes_remaining;
    if (m >= 60) {
      snprintf(buf, sizeof(buf), "resets %s - %dh %dm left",
               s.session.resets_at.c_str(), m / 60, m % 60);
    } else {
      snprintf(buf, sizeof(buf), "resets %s - %dm left",
               s.session.resets_at.c_str(), m);
    }
    lv_label_set_text(session_meta_, buf);
  } else {
    lv_label_set_text(session_meta_, "no active session");
  }

  int wp = s.budgets.code_all;
  snprintf(buf, sizeof(buf), "%d%%", wp);
  lv_label_set_text(week_pct_, buf);
  set_bar(week_bar_, wp);

  if (!s.budgets.resets_in.empty()) {
    snprintf(buf, sizeof(buf), "resets in %s", s.budgets.resets_in.c_str());
    lv_label_set_text(week_meta_, buf);
  } else {
    lv_label_set_text(week_meta_, "-");
  }

  int t = s.models_today.total_tokens;
  if (t >= 1000000) {
    snprintf(buf, sizeof(buf), "%.1fM - $%.2f",
             t / 1000000.0, s.models_today.est_cost_usd);
  } else {
    snprintf(buf, sizeof(buf), "%dk - $%.2f",
             t / 1000, s.models_today.est_cost_usd);
  }
  lv_label_set_text(today_line_, buf);
}

}  // namespace cyd

#endif  // UNIT_TEST
