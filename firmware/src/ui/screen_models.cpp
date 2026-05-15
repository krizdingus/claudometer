#include "ui/screen_models.h"

#ifndef UNIT_TEST

#include <stdio.h>
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

void build_row(lv_obj_t *parent, int y_offset, const char *name,
               ScreenModels::Row &r) {
  r.label = lv_label_create(parent);
  lv_label_set_text(r.label, name);
  lv_obj_set_style_text_color(r.label, theme::fg(), 0);
  lv_obj_set_style_text_font(r.label, &lv_font_montserrat_14, 0);
  lv_obj_align(r.label, LV_ALIGN_TOP_LEFT, 4, y_offset);

  r.tokens = lv_label_create(parent);
  lv_obj_set_style_text_color(r.tokens, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(r.tokens, &lv_font_montserrat_12, 0);
  lv_obj_align(r.tokens, LV_ALIGN_TOP_LEFT, 70, y_offset + 2);

  r.bar = lv_bar_create(parent);
  lv_obj_set_size(r.bar, 100, 6);
  lv_obj_align(r.bar, LV_ALIGN_TOP_RIGHT, -4, y_offset + 5);
  lv_bar_set_range(r.bar, 0, 1000000);
  lv_obj_set_style_bg_color(r.bar, theme::bar_bg(), LV_PART_MAIN);
  lv_obj_set_style_bg_color(r.bar, theme::accent(), LV_PART_INDICATOR);
  lv_obj_set_style_radius(r.bar, 3, LV_PART_MAIN);
  lv_obj_set_style_radius(r.bar, 3, LV_PART_INDICATOR);
}

}  // namespace

void ScreenModels::build(lv_obj_t *parent) {
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Today");
  lv_obj_set_style_text_color(title, theme::fg(), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

  cost_ = lv_label_create(parent);
  lv_obj_set_style_text_color(cost_, theme::fg(), 0);
  lv_obj_set_style_text_font(cost_, &lv_font_montserrat_48, 0);
  lv_obj_align(cost_, LV_ALIGN_TOP_LEFT, 4, 24);
  lv_label_set_text(cost_, "$0.00");

  total_ = lv_label_create(parent);
  lv_obj_set_style_text_color(total_, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(total_, &lv_font_montserrat_14, 0);
  lv_obj_align(total_, LV_ALIGN_TOP_LEFT, 4, 82);
  lv_label_set_text(total_, "0k tokens");

  hairline(parent, 106);

  build_row(parent, 118, "Opus",   opus_);
  build_row(parent, 152, "Sonnet", sonnet_);
  build_row(parent, 186, "Haiku",  haiku_);
}

static int find_tokens(const std::vector<ModelTokenRow> &rows, const char *needle) {
  for (const auto &r : rows) {
    if (r.model.find(needle) != std::string::npos) return r.tokens;
  }
  return 0;
}

void ScreenModels::update(const Stats &s) {
  char buf[40];
  snprintf(buf, sizeof(buf), "$%.2f", s.models_today.est_cost_usd);
  lv_label_set_text(cost_, buf);

  int t = s.models_today.total_tokens;
  if (t >= 1000000) {
    snprintf(buf, sizeof(buf), "%.1fM tokens", t / 1000000.0);
  } else {
    snprintf(buf, sizeof(buf), "%dk tokens", t / 1000);
  }
  lv_label_set_text(total_, buf);

  int opus = find_tokens(s.models_today.by_model, "opus");
  int sonnet = find_tokens(s.models_today.by_model, "sonnet");
  int haiku = find_tokens(s.models_today.by_model, "haiku");

  int cap = std::max({opus, sonnet, haiku, 1});
  lv_bar_set_range(opus_.bar, 0, cap);
  lv_bar_set_range(sonnet_.bar, 0, cap);
  lv_bar_set_range(haiku_.bar, 0, cap);

  auto fill = [](Row &r, int tokens) {
    lv_bar_set_value(r.bar, tokens, LV_ANIM_OFF);
    char b[24];
    snprintf(b, sizeof(b), "%dk", tokens / 1000);
    lv_label_set_text(r.tokens, b);
  };
  fill(opus_, opus);
  fill(sonnet_, sonnet);
  fill(haiku_, haiku);
}

}  // namespace cyd

#endif  // UNIT_TEST
