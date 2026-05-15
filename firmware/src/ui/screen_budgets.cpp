#include "ui/screen_budgets.h"

#ifndef UNIT_TEST

#include <stdio.h>
#include <algorithm>

#include "ui/theme.h"

namespace cyd {

static void build_row(lv_obj_t *parent, int y, const char *name,
                      ScreenBudgets::Row &r) {
  r.label = lv_label_create(parent);
  lv_label_set_text(r.label, name);
  lv_obj_set_style_text_color(r.label, theme::fg(), 0);
  lv_obj_set_style_text_font(r.label, &lv_font_montserrat_14, 0);
  lv_obj_align(r.label, LV_ALIGN_TOP_LEFT, 4, y);

  r.bar = lv_bar_create(parent);
  lv_obj_set_size(r.bar, 150, 14);
  lv_obj_align(r.bar, LV_ALIGN_TOP_LEFT, 60, y + 2);
  lv_bar_set_range(r.bar, 0, 100);
  lv_obj_set_style_bg_color(r.bar, theme::bar_bg(), LV_PART_MAIN);
  lv_obj_set_style_bg_color(r.bar, theme::accent(), LV_PART_INDICATOR);
  lv_obj_set_style_radius(r.bar, 6, LV_PART_MAIN);
  lv_obj_set_style_radius(r.bar, 6, LV_PART_INDICATOR);

  r.pct = lv_label_create(parent);
  lv_obj_set_style_text_color(r.pct, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(r.pct, &lv_font_montserrat_12, 0);
  lv_obj_align(r.pct, LV_ALIGN_TOP_RIGHT, -4, y + 2);
}

void ScreenBudgets::build(lv_obj_t *parent) {
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Budgets - This Week");
  lv_obj_set_style_text_color(title, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

  build_row(parent, 24, "Code",  code_all_);
  build_row(parent, 60, "Opus",  code_opus_);

  plan_ = lv_label_create(parent);
  lv_obj_set_style_text_color(plan_, theme::accent(), 0);
  lv_obj_set_style_text_font(plan_, &lv_font_montserrat_16, 0);
  lv_obj_align(plan_, LV_ALIGN_TOP_LEFT, 4, 120);

  resets_ = lv_label_create(parent);
  lv_obj_set_style_text_color(resets_, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(resets_, &lv_font_montserrat_14, 0);
  lv_obj_align(resets_, LV_ALIGN_TOP_LEFT, 4, 148);

  warn_ = lv_label_create(parent);
  lv_obj_set_style_text_color(warn_, theme::bg(), 0);
  lv_obj_set_style_text_font(warn_, &lv_font_montserrat_12, 0);
  lv_obj_set_style_bg_color(warn_, theme::alert(), 0);
  lv_obj_set_style_bg_opa(warn_, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(warn_, 4, 0);
  lv_obj_set_style_radius(warn_, 6, 0);
  lv_obj_align(warn_, LV_ALIGN_TOP_LEFT, 4, 180);
  lv_obj_add_flag(warn_, LV_OBJ_FLAG_HIDDEN);
}

static void set_row(ScreenBudgets::Row &r, int pct) {
  lv_bar_set_value(r.bar, pct, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(r.bar, theme::bar_color_for_pct(pct), LV_PART_INDICATOR);
  char b[8];
  snprintf(b, sizeof(b), "%d%%", pct);
  lv_label_set_text(r.pct, b);
}

void ScreenBudgets::update(const Stats &s) {
  set_row(code_all_, s.budgets.code_all);
  set_row(code_opus_, s.budgets.code_opus);

  lv_label_set_text(plan_, s.budgets.plan.empty() ? "-" : s.budgets.plan.c_str());
  char r[40];
  snprintf(r, sizeof(r), "resets in %s", s.budgets.resets_in.c_str());
  lv_label_set_text(resets_, r);

  int worst = std::max(s.budgets.code_all, s.budgets.code_opus);
  if (worst >= 85) {
    char w[40];
    snprintf(w, sizeof(w), "  %d%% used - slow down  ", worst);
    lv_label_set_text(warn_, w);
    lv_obj_clear_flag(warn_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(warn_, LV_OBJ_FLAG_HIDDEN);
  }
}

} // namespace cyd

#endif  // UNIT_TEST
