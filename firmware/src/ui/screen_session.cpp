#include "ui/screen_session.h"

#ifndef UNIT_TEST

#include <stdio.h>

#include "ui/theme.h"

namespace cyd {

void ScreenSession::build(lv_obj_t *parent) {
  // Section title
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Session");
  lv_obj_set_style_text_color(title, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 4);

  // 5h block ring
  arc_ = lv_arc_create(parent);
  lv_obj_set_size(arc_, 160, 160);
  lv_obj_align(arc_, LV_ALIGN_TOP_MID, 0, 22);
  lv_arc_set_rotation(arc_, 270);
  lv_arc_set_bg_angles(arc_, 0, 360);
  lv_arc_set_range(arc_, 0, 100);
  lv_obj_remove_style(arc_, NULL, LV_PART_KNOB);
  lv_obj_set_style_arc_width(arc_, 14, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc_, 14, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc_, theme::c(0x222226), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc_, theme::c(theme::accent), LV_PART_INDICATOR);
  lv_obj_clear_flag(arc_, LV_OBJ_FLAG_CLICKABLE);

  // Center: big % number
  pct_label_ = lv_label_create(arc_);
  lv_obj_set_style_text_color(pct_label_, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(pct_label_, &lv_font_montserrat_32, 0);
  lv_label_set_text(pct_label_, "--%");
  lv_obj_center(pct_label_);

  // Resets at label below the ring
  resets_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(resets_label_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(resets_label_, &lv_font_montserrat_14, 0);
  lv_obj_align(resets_label_, LV_ALIGN_TOP_MID, 0, 188);
  lv_label_set_text(resets_label_, "resets --:--");

  // Top 2 models
  model_a_ = lv_label_create(parent);
  model_b_ = lv_label_create(parent);
  for (auto *m : {model_a_, model_b_}) {
    lv_obj_set_style_text_color(m, theme::c(theme::fg), 0);
    lv_obj_set_style_text_font(m, &lv_font_montserrat_14, 0);
  }
  lv_obj_align(model_a_, LV_ALIGN_TOP_LEFT, 8, 218);
  lv_obj_align(model_b_, LV_ALIGN_TOP_LEFT, 8, 240);
  lv_label_set_text(model_a_, "");
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
  char buf[16];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  lv_label_set_text(pct_label_, buf);

  if (!s.session.resets_at.empty()) {
    char r[40];
    snprintf(r, sizeof(r), "resets %s, %dm left",
             s.session.resets_at.c_str(), s.session.minutes_remaining);
    lv_label_set_text(resets_label_, r);
  } else {
    lv_label_set_text(resets_label_, "no active session");
  }

  auto fmt_row = [](lv_obj_t *lbl, const ModelTokenRow *row) {
    if (!row) { lv_label_set_text(lbl, ""); return; }
    char r[40];
    snprintf(r, sizeof(r), "%s  %dk", short_name(row->model), row->tokens / 1000);
    lv_label_set_text(lbl, r);
  };
  fmt_row(model_a_, s.session.models.size() > 0 ? &s.session.models[0] : nullptr);
  fmt_row(model_b_, s.session.models.size() > 1 ? &s.session.models[1] : nullptr);
}

} // namespace cyd

#endif  // UNIT_TEST
