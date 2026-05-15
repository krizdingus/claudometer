#include "ui/screen_budgets.h"

#ifndef UNIT_TEST

#include <stdio.h>

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

}  // namespace

void ScreenBudgets::build(lv_obj_t *parent) {
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Caps");
  lv_obj_set_style_text_color(title, theme::fg(), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

  // ALL MODELS — hero
  auto *all_label = lv_label_create(parent);
  lv_label_set_text(all_label, "ALL MODELS");
  lv_obj_set_style_text_color(all_label, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(all_label, &lv_font_montserrat_12, 0);
  lv_obj_align(all_label, LV_ALIGN_TOP_LEFT, 4, 26);

  hero_pct_ = lv_label_create(parent);
  lv_obj_set_style_text_color(hero_pct_, theme::fg(), 0);
  lv_obj_set_style_text_font(hero_pct_, &lv_font_montserrat_48, 0);
  lv_obj_align(hero_pct_, LV_ALIGN_TOP_LEFT, 4, 42);
  lv_label_set_text(hero_pct_, "--%");

  hero_bar_ = lv_bar_create(parent);
  lv_obj_set_size(hero_bar_, 224, 8);
  lv_obj_align(hero_bar_, LV_ALIGN_TOP_LEFT, 4, 100);
  lv_bar_set_range(hero_bar_, 0, 100);
  lv_obj_set_style_bg_color(hero_bar_, theme::bar_bg(), LV_PART_MAIN);
  lv_obj_set_style_bg_color(hero_bar_, theme::accent(), LV_PART_INDICATOR);
  lv_obj_set_style_radius(hero_bar_, 4, LV_PART_MAIN);
  lv_obj_set_style_radius(hero_bar_, 4, LV_PART_INDICATOR);

  hairline(parent, 124);

  // OPUS — secondary row
  code_opus_.label = lv_label_create(parent);
  lv_label_set_text(code_opus_.label, "OPUS");
  lv_obj_set_style_text_color(code_opus_.label, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(code_opus_.label, &lv_font_montserrat_12, 0);
  lv_obj_align(code_opus_.label, LV_ALIGN_TOP_LEFT, 4, 134);

  code_opus_.pct = lv_label_create(parent);
  lv_obj_set_style_text_color(code_opus_.pct, theme::fg(), 0);
  lv_obj_set_style_text_font(code_opus_.pct, &lv_font_montserrat_14, 0);
  lv_obj_align(code_opus_.pct, LV_ALIGN_TOP_RIGHT, -4, 134);

  code_opus_.bar = lv_bar_create(parent);
  lv_obj_set_size(code_opus_.bar, 224, 6);
  lv_obj_align(code_opus_.bar, LV_ALIGN_TOP_LEFT, 4, 154);
  lv_bar_set_range(code_opus_.bar, 0, 100);
  lv_obj_set_style_bg_color(code_opus_.bar, theme::bar_bg(), LV_PART_MAIN);
  lv_obj_set_style_bg_color(code_opus_.bar, theme::accent(), LV_PART_INDICATOR);
  lv_obj_set_style_radius(code_opus_.bar, 3, LV_PART_MAIN);
  lv_obj_set_style_radius(code_opus_.bar, 3, LV_PART_INDICATOR);

  hairline(parent, 180);

  plan_ = lv_label_create(parent);
  lv_obj_set_style_text_color(plan_, theme::fg(), 0);
  lv_obj_set_style_text_font(plan_, &lv_font_montserrat_14, 0);
  lv_obj_align(plan_, LV_ALIGN_TOP_LEFT, 4, 190);

  resets_ = lv_label_create(parent);
  lv_obj_set_style_text_color(resets_, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(resets_, &lv_font_montserrat_12, 0);
  lv_obj_align(resets_, LV_ALIGN_TOP_RIGHT, -4, 192);

  // code_all_ row is no longer rendered — its data goes into hero_pct_/hero_bar_
  code_all_.label = nullptr;
  code_all_.bar = nullptr;
  code_all_.pct = nullptr;
}

static const char *pretty_plan(const std::string &raw) {
  if (raw == "max-20x") return "MAX 20x";
  if (raw == "max-5x")  return "MAX 5x";
  if (raw == "pro")     return "PRO";
  if (raw == "free")    return "FREE";
  return raw.c_str();
}

void ScreenBudgets::update(const Stats &s) {
  int all = s.budgets.code_all;
  char buf[16];
  snprintf(buf, sizeof(buf), "%d%%", all);
  lv_label_set_text(hero_pct_, buf);
  lv_bar_set_value(hero_bar_, all, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(hero_bar_, theme::bar_color_for_pct(all), LV_PART_INDICATOR);

  int opus = s.budgets.code_opus;
  snprintf(buf, sizeof(buf), "%d%%", opus);
  lv_label_set_text(code_opus_.pct, buf);
  lv_bar_set_value(code_opus_.bar, opus, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(code_opus_.bar, theme::bar_color_for_pct(opus), LV_PART_INDICATOR);

  lv_label_set_text(plan_, pretty_plan(s.budgets.plan));

  char r[40];
  if (!s.budgets.resets_in.empty()) {
    snprintf(r, sizeof(r), "resets in %s", s.budgets.resets_in.c_str());
    lv_label_set_text(resets_, r);
  } else {
    lv_label_set_text(resets_, "");
  }
}

}  // namespace cyd

#endif  // UNIT_TEST
