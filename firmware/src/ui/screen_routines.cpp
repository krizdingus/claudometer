#include "ui/screen_routines.h"

#ifndef UNIT_TEST

#include <climits>
#include <stdio.h>

#include "ui/theme.h"

namespace cyd {

void ScreenRoutines::build(lv_obj_t *parent) {
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Routines");
  lv_obj_set_style_text_color(title, theme::fg(), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

  // NEXT hero block (24pt — "how soon is the next thing" hero, not a
  // single dominant number like other screens).
  next_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(next_label_, theme::fg(), 0);
  lv_obj_set_style_text_font(next_label_, &lv_font_montserrat_24, 0);
  lv_obj_align(next_label_, LV_ALIGN_TOP_LEFT, 4, 24);
  lv_label_set_text(next_label_, "NEXT —");

  next_name_ = lv_label_create(parent);
  lv_obj_set_style_text_color(next_name_, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(next_name_, &lv_font_montserrat_14, 0);
  lv_obj_align(next_name_, LV_ALIGN_TOP_LEFT, 4, 56);
  lv_label_set_text(next_name_, "");

  // Hairline at y=82, then rows below
  auto *h = lv_obj_create(parent);
  lv_obj_set_size(h, 224, 1);
  lv_obj_align(h, LV_ALIGN_TOP_LEFT, 4, 82);
  lv_obj_set_style_bg_color(h, theme::bar_bg(), 0);
  lv_obj_set_style_bg_opa(h, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(h, 0, 0);
  lv_obj_set_style_radius(h, 0, 0);

  for (int i = 0; i < kMaxRows; ++i) {
    int y = 92 + i * 56;  // 56px per row (name+when+gap)
    rows_[i].name = lv_label_create(parent);
    lv_obj_set_style_text_color(rows_[i].name, theme::fg(), 0);
    lv_obj_set_style_text_font(rows_[i].name, &lv_font_montserrat_14, 0);
    lv_obj_align(rows_[i].name, LV_ALIGN_TOP_LEFT, 4, y);
    lv_obj_add_flag(rows_[i].name, LV_OBJ_FLAG_HIDDEN);

    rows_[i].pill = lv_obj_create(parent);
    lv_obj_set_size(rows_[i].pill, 56, 18);
    lv_obj_align(rows_[i].pill, LV_ALIGN_TOP_RIGHT, -4, y - 2);
    lv_obj_set_style_radius(rows_[i].pill, 9, 0);
    lv_obj_set_style_border_width(rows_[i].pill, 0, 0);
    lv_obj_clear_flag(rows_[i].pill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(rows_[i].pill, LV_OBJ_FLAG_HIDDEN);

    auto *pill_label = lv_label_create(rows_[i].pill);
    lv_obj_set_style_text_color(pill_label, theme::bg(), 0);
    lv_obj_set_style_text_font(pill_label, &lv_font_montserrat_12, 0);
    lv_obj_center(pill_label);
    lv_obj_set_user_data(rows_[i].pill, pill_label);

    rows_[i].when = lv_label_create(parent);
    lv_obj_set_style_text_color(rows_[i].when, theme::fg_muted(), 0);
    lv_obj_set_style_text_font(rows_[i].when, &lv_font_montserrat_12, 0);
    lv_obj_align(rows_[i].when, LV_ALIGN_TOP_LEFT, 4, y + 20);
    lv_obj_add_flag(rows_[i].when, LV_OBJ_FLAG_HIDDEN);
  }

  empty_ = lv_label_create(parent);
  lv_obj_set_style_text_color(empty_, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(empty_, &lv_font_montserrat_14, 0);
  lv_label_set_long_mode(empty_, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(empty_, 220);
  lv_obj_align(empty_, LV_ALIGN_TOP_LEFT, 4, 90);
  lv_label_set_text(empty_, "No routines yet.\nRun `claude routines add`\nto get started.");
}

void ScreenRoutines::update(const Stats &s) {
  bool any = !s.routines.empty();

  if (any) {
    lv_obj_add_flag(empty_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(next_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(next_name_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(empty_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(next_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(next_name_, LV_OBJ_FLAG_HIDDEN);
  }

  if (any) {
    int best_idx = -1;
    int best_mins = INT32_MAX;
    for (size_t i = 0; i < s.routines.size(); ++i) {
      int m = s.routines[i].next_run_in_minutes;
      if (m >= 0 && m < best_mins) {
        best_mins = m;
        best_idx = (int)i;
      }
    }
    if (best_idx >= 0) {
      char buf[32];
      if (best_mins >= 60) {
        snprintf(buf, sizeof(buf), "NEXT  %dh %dm", best_mins / 60, best_mins % 60);
      } else {
        snprintf(buf, sizeof(buf), "NEXT  %dm", best_mins);
      }
      lv_label_set_text(next_label_, buf);
      lv_label_set_text(next_name_, s.routines[best_idx].name.c_str());
    } else {
      lv_label_set_text(next_label_, "NEXT  —");
      lv_label_set_text(next_name_, "no upcoming");
    }
  }

  for (int i = 0; i < kMaxRows; ++i) {
    bool show = i < (int)s.routines.size();
    auto vis = [show](lv_obj_t *o) {
      if (show) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
      else      lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    };
    vis(rows_[i].name);
    vis(rows_[i].pill);
    vis(rows_[i].when);
    if (!show) continue;

    const auto &r = s.routines[i];
    lv_label_set_text(rows_[i].name, r.name.c_str());
    lv_obj_set_style_bg_color(rows_[i].pill,
                              theme::status_pill_for(r.status.c_str()), 0);
    auto *pill_label =
        static_cast<lv_obj_t *>(lv_obj_get_user_data(rows_[i].pill));
    lv_label_set_text(pill_label, r.status.c_str());

    char w[40];
    snprintf(w, sizeof(w), "last %s · next %s",
             r.last_run.c_str(), r.next_run.c_str());
    lv_label_set_text(rows_[i].when, w);
  }
}

} // namespace cyd

#endif  // UNIT_TEST
