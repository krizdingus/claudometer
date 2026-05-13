#include "ui/screen_session.h"

#ifndef UNIT_TEST

#include <stdio.h>
#include <vector>
#include <algorithm>

#include "ui/theme.h"

namespace cyd {

void ScreenSession::build(lv_obj_t *parent) {
  // Section title
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Current session");
  lv_obj_set_style_text_color(title, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 2);

  // Block-end stamp at top-right ("18:43")
  resets_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(resets_label_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(resets_label_, &lv_font_montserrat_12, 0);
  lv_obj_align(resets_label_, LV_ALIGN_TOP_RIGHT, -4, 2);
  lv_label_set_text(resets_label_, "resets --:--");

  // 5h block ring (a touch bigger and lower so it owns the screen)
  arc_ = lv_arc_create(parent);
  lv_obj_set_size(arc_, 180, 180);
  lv_obj_align(arc_, LV_ALIGN_TOP_MID, 0, 26);
  lv_arc_set_rotation(arc_, 270);
  lv_arc_set_bg_angles(arc_, 0, 360);
  lv_arc_set_range(arc_, 0, 100);
  lv_obj_remove_style(arc_, NULL, LV_PART_KNOB);
  lv_obj_set_style_arc_width(arc_, 16, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc_, 16, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc_, theme::c(0x1F1F22), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc_, theme::c(theme::accent), LV_PART_INDICATOR);
  lv_obj_clear_flag(arc_, LV_OBJ_FLAG_CLICKABLE);

  // Center: % used (hero number)
  pct_label_ = lv_label_create(arc_);
  lv_obj_set_style_text_color(pct_label_, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(pct_label_, &lv_font_montserrat_48, 0);
  lv_label_set_text(pct_label_, "--%");
  lv_obj_align(pct_label_, LV_ALIGN_CENTER, 0, -8);

  // Under the % inside the ring: time remaining
  model_a_ = lv_label_create(arc_);
  lv_obj_set_style_text_color(model_a_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(model_a_, &lv_font_montserrat_14, 0);
  lv_label_set_text(model_a_, "");
  lv_obj_align(model_a_, LV_ALIGN_CENTER, 0, 28);

  // Models breakdown row at the bottom (e.g. "Opus 220k  Sonnet 90k")
  model_b_ = lv_label_create(parent);
  lv_obj_set_style_text_color(model_b_, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(model_b_, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(model_b_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(model_b_, 224);
  lv_obj_align(model_b_, LV_ALIGN_TOP_MID, 0, 232);
  lv_label_set_text(model_b_, "");
}

static const char *short_name(const std::string &m) {
  if (m.find("opus") != std::string::npos)   return "Opus";
  if (m.find("sonnet") != std::string::npos) return "Sonnet";
  if (m.find("haiku") != std::string::npos)  return "Haiku";
  return m.c_str();
}

void ScreenSession::update(const Stats &s) {
  int pct = s.session.pct_used;
  lv_arc_set_value(arc_, pct);
  lv_obj_set_style_arc_color(arc_, theme::bar_color_for_pct(pct), LV_PART_INDICATOR);
  char buf[40];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  lv_label_set_text(pct_label_, buf);

  if (!s.session.resets_at.empty()) {
    int m = s.session.minutes_remaining;
    if (m >= 60) {
      snprintf(buf, sizeof(buf), "%dh %dm left", m / 60, m % 60);
    } else {
      snprintf(buf, sizeof(buf), "%dm left", m);
    }
    lv_label_set_text(model_a_, buf);

    char r[24];
    snprintf(r, sizeof(r), "resets %s", s.session.resets_at.c_str());
    lv_label_set_text(resets_label_, r);
  } else {
    lv_label_set_text(model_a_, "no active session");
    lv_label_set_text(resets_label_, "");
  }

  // Bottom row: top 2 models inline, comma-separated.
  std::string row;
  for (size_t i = 0; i < s.session.models.size() && i < 2; ++i) {
    const auto &m = s.session.models[i];
    if (m.tokens <= 0) continue;
    if (!row.empty()) row += "   ";
    char part[24];
    if (m.tokens >= 1000) {
      snprintf(part, sizeof(part), "%s %dk", short_name(m.model), m.tokens / 1000);
    } else {
      snprintf(part, sizeof(part), "%s %d", short_name(m.model), m.tokens);
    }
    row += part;
  }
  lv_label_set_text(model_b_, row.c_str());
}

}  // namespace cyd

#endif  // UNIT_TEST
