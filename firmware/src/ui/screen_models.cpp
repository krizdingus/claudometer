#include "ui/screen_models.h"

#ifndef UNIT_TEST

#include <stdio.h>
#include <algorithm>

#include "ui/theme.h"

namespace cyd {

static void build_row(lv_obj_t *parent, int y_offset, const char *name,
                      ScreenModels::Row &r) {
  r.label = lv_label_create(parent);
  lv_label_set_text(r.label, name);
  lv_obj_set_style_text_color(r.label, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(r.label, &lv_font_montserrat_14, 0);
  lv_obj_align(r.label, LV_ALIGN_TOP_LEFT, 4, y_offset);

  r.bar = lv_bar_create(parent);
  lv_obj_set_size(r.bar, 160, 14);
  lv_obj_align(r.bar, LV_ALIGN_TOP_LEFT, 60, y_offset + 2);
  lv_bar_set_range(r.bar, 0, 1000000);
  lv_obj_set_style_bg_color(r.bar, theme::c(0x222226), LV_PART_MAIN);
  lv_obj_set_style_bg_color(r.bar, theme::c(theme::blue), LV_PART_INDICATOR);
  lv_obj_set_style_radius(r.bar, 6, LV_PART_MAIN);
  lv_obj_set_style_radius(r.bar, 6, LV_PART_INDICATOR);

  r.tokens = lv_label_create(parent);
  lv_obj_set_style_text_color(r.tokens, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(r.tokens, &lv_font_montserrat_12, 0);
  lv_obj_align(r.tokens, LV_ALIGN_TOP_LEFT, 60, y_offset + 20);
}

void ScreenModels::build(lv_obj_t *parent) {
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "All Models · Today");
  lv_obj_set_style_text_color(title, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

  build_row(parent, 22, "Opus",   opus_);
  build_row(parent, 76, "Sonnet", sonnet_);
  build_row(parent, 130, "Haiku", haiku_);

  total_ = lv_label_create(parent);
  lv_obj_set_style_text_color(total_, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(total_, &lv_font_montserrat_16, 0);
  lv_obj_align(total_, LV_ALIGN_TOP_LEFT, 4, 200);

  cost_ = lv_label_create(parent);
  lv_obj_set_style_text_color(cost_, theme::c(theme::accent), 0);
  lv_obj_set_style_text_font(cost_, &lv_font_montserrat_24, 0);
  lv_obj_align(cost_, LV_ALIGN_TOP_LEFT, 4, 222);
}

static int find_tokens(const std::vector<ModelTokenRow> &rows, const char *needle) {
  for (const auto &r : rows) {
    if (r.model.find(needle) != std::string::npos) return r.tokens;
  }
  return 0;
}

void ScreenModels::update(const Stats &s) {
  int opus = find_tokens(s.models_today.by_model, "opus");
  int sonnet = find_tokens(s.models_today.by_model, "sonnet");
  int haiku = find_tokens(s.models_today.by_model, "haiku");

  // Scale bars relative to the largest, so the visual is comparative.
  int cap = std::max({opus, sonnet, haiku, 1});
  lv_bar_set_range(opus_.bar, 0, cap);
  lv_bar_set_range(sonnet_.bar, 0, cap);
  lv_bar_set_range(haiku_.bar, 0, cap);

  auto fill = [](Row &r, int tokens) {
    lv_bar_set_value(r.bar, tokens, LV_ANIM_OFF);
    char buf[24];
    snprintf(buf, sizeof(buf), "%dk tokens", tokens / 1000);
    lv_label_set_text(r.tokens, buf);
  };
  fill(opus_, opus);
  fill(sonnet_, sonnet);
  fill(haiku_, haiku);

  char buf[40];
  snprintf(buf, sizeof(buf), "%dk total", s.models_today.total_tokens / 1000);
  lv_label_set_text(total_, buf);
  snprintf(buf, sizeof(buf), "$%.2f est.", s.models_today.est_cost_usd);
  lv_label_set_text(cost_, buf);
}

} // namespace cyd

#endif  // UNIT_TEST
