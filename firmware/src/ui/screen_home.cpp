#include "ui/screen_home.h"

#ifndef UNIT_TEST

#include <stdio.h>
#include <string.h>
#include <algorithm>

#include "ui/theme.h"

namespace cyd {

namespace {

constexpr int kBarHeight = 12;
constexpr int kBarRadius = 6;

const char *pretty_plan(const std::string &raw) {
  if (raw == "max-20x") return "MAX 20x";
  if (raw == "max-5x")  return "MAX 5x";
  if (raw == "pro")     return "PRO";
  if (raw == "free")    return "FREE";
  return raw.c_str();
}

lv_obj_t *section_header(lv_obj_t *parent, const char *text, int y) {
  auto *lbl = lv_label_create(parent);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_color(lbl, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 4, y);
  return lbl;
}

lv_obj_t *make_bar(lv_obj_t *parent, int y) {
  auto *b = lv_bar_create(parent);
  lv_obj_set_size(b, 224, kBarHeight);
  lv_obj_align(b, LV_ALIGN_TOP_LEFT, 4, y);
  lv_bar_set_range(b, 0, 100);
  lv_obj_set_style_bg_color(b, theme::c(0x1F1F22), LV_PART_MAIN);
  lv_obj_set_style_bg_color(b, theme::c(theme::blue), LV_PART_INDICATOR);
  lv_obj_set_style_radius(b, kBarRadius, LV_PART_MAIN);
  lv_obj_set_style_radius(b, kBarRadius, LV_PART_INDICATOR);
  return b;
}

}  // namespace

void ScreenHome::build(lv_obj_t *parent) {
  // Plan pill, top-right.
  plan_pill_ = lv_obj_create(parent);
  lv_obj_set_size(plan_pill_, 78, 22);
  lv_obj_align(plan_pill_, LV_ALIGN_TOP_RIGHT, -4, 0);
  lv_obj_set_style_bg_color(plan_pill_, theme::c(theme::accent), 0);
  lv_obj_set_style_bg_opa(plan_pill_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(plan_pill_, 0, 0);
  lv_obj_set_style_radius(plan_pill_, 11, 0);
  lv_obj_set_style_pad_all(plan_pill_, 0, 0);
  lv_obj_clear_flag(plan_pill_, LV_OBJ_FLAG_SCROLLABLE);

  plan_pill_label_ = lv_label_create(plan_pill_);
  lv_obj_set_style_text_color(plan_pill_label_, theme::c(theme::bg), 0);
  lv_obj_set_style_text_font(plan_pill_label_, &lv_font_montserrat_14, 0);
  lv_label_set_text(plan_pill_label_, "—");
  lv_obj_center(plan_pill_label_);

  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Usage");
  lv_obj_set_style_text_color(title, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 2);

  // SESSION block
  section_header(parent, "SESSION", 36);
  session_pct_ = lv_label_create(parent);
  lv_obj_set_style_text_color(session_pct_, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(session_pct_, &lv_font_montserrat_32, 0);
  lv_obj_align(session_pct_, LV_ALIGN_TOP_RIGHT, -4, 28);
  lv_label_set_text(session_pct_, "--%");

  session_bar_ = make_bar(parent, 72);

  session_meta_ = lv_label_create(parent);
  lv_obj_set_style_text_color(session_meta_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(session_meta_, &lv_font_montserrat_12, 0);
  lv_obj_align(session_meta_, LV_ALIGN_TOP_LEFT, 4, 90);
  lv_label_set_text(session_meta_, "no active session");

  // WEEK block
  section_header(parent, "WEEK", 122);
  week_pct_ = lv_label_create(parent);
  lv_obj_set_style_text_color(week_pct_, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(week_pct_, &lv_font_montserrat_32, 0);
  lv_obj_align(week_pct_, LV_ALIGN_TOP_RIGHT, -4, 114);
  lv_label_set_text(week_pct_, "--%");

  week_bar_ = make_bar(parent, 158);

  week_meta_ = lv_label_create(parent);
  lv_obj_set_style_text_color(week_meta_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(week_meta_, &lv_font_montserrat_12, 0);
  lv_obj_align(week_meta_, LV_ALIGN_TOP_LEFT, 4, 176);
  lv_label_set_text(week_meta_, "—");

  // TODAY summary line
  auto *today_label = lv_label_create(parent);
  lv_label_set_text(today_label, "TODAY");
  lv_obj_set_style_text_color(today_label, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(today_label, &lv_font_montserrat_12, 0);
  lv_obj_align(today_label, LV_ALIGN_TOP_LEFT, 4, 208);

  today_line_ = lv_label_create(parent);
  lv_obj_set_style_text_color(today_line_, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(today_line_, &lv_font_montserrat_16, 0);
  lv_obj_align(today_line_, LV_ALIGN_TOP_LEFT, 4, 226);
  lv_label_set_text(today_line_, "0k tokens");
}

static void set_bar(lv_obj_t *bar, int pct) {
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  lv_bar_set_value(bar, pct, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar, theme::bar_color_for_pct(pct), LV_PART_INDICATOR);
}

void ScreenHome::update(const Stats &s) {
  lv_label_set_text(plan_pill_label_, pretty_plan(s.budgets.plan));

  int sp = s.session.pct_used;
  char buf[64];
  snprintf(buf, sizeof(buf), "%d%%", sp);
  lv_label_set_text(session_pct_, buf);
  set_bar(session_bar_, sp);

  if (!s.session.resets_at.empty()) {
    int m = s.session.minutes_remaining;
    if (m >= 60) {
      snprintf(buf, sizeof(buf), "resets %s  -  %dh %dm left",
               s.session.resets_at.c_str(), m / 60, m % 60);
    } else {
      snprintf(buf, sizeof(buf), "resets %s  -  %dm left",
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
    snprintf(buf, sizeof(buf), "all models  -  resets in %s",
             s.budgets.resets_in.c_str());
    lv_label_set_text(week_meta_, buf);
  } else {
    lv_label_set_text(week_meta_, "all models");
  }

  int t = s.models_today.total_tokens;
  if (t >= 1000000) {
    snprintf(buf, sizeof(buf), "%.1fM tokens  -  $%.2f",
             t / 1000000.0, s.models_today.est_cost_usd);
  } else {
    snprintf(buf, sizeof(buf), "%dk tokens  -  $%.2f",
             t / 1000, s.models_today.est_cost_usd);
  }
  lv_label_set_text(today_line_, buf);
}

}  // namespace cyd

#endif  // UNIT_TEST
