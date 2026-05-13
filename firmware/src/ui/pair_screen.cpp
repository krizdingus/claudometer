#include "ui/pair_screen.h"

#ifndef UNIT_TEST

#include "ui/theme.h"

namespace cyd {

void PairScreen::build(lv_obj_t *parent) {
  theme::apply_screen_styles(parent);

  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Pair with daemon");
  lv_obj_set_style_text_color(title, theme::c(theme::accent), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

  host_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(host_label_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(host_label_, &lv_font_montserrat_14, 0);
  lv_obj_align(host_label_, LV_ALIGN_TOP_MID, 0, 56);
  lv_label_set_text(host_label_, "");

  code_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(code_label_, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(code_label_, &lv_font_montserrat_32, 0);
  lv_obj_align(code_label_, LV_ALIGN_TOP_MID, 0, 90);
  lv_label_set_text(code_label_, "----");

  auto *hint = lv_label_create(parent);
  lv_obj_set_style_text_color(hint, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
  lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 138);
  lv_label_set_text(hint, "Check the daemon's tray icon\nor run `cydmonitor status`");

  button_ = lv_btn_create(parent);
  lv_obj_set_size(button_, 200, 48);
  lv_obj_align(button_, LV_ALIGN_TOP_MID, 0, 200);
  lv_obj_set_style_bg_color(button_, theme::c(theme::accent), 0);
  lv_obj_set_style_radius(button_, 24, 0);
  auto *btn_label = lv_label_create(button_);
  lv_label_set_text(btn_label, "Confirm");
  lv_obj_set_style_text_color(btn_label, theme::c(theme::bg), 0);
  lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_16, 0);
  lv_obj_center(btn_label);
  lv_obj_add_event_cb(button_, &PairScreen::btn_event, LV_EVENT_CLICKED, this);
}

void PairScreen::set_code(const char *c) {
  if (c) lv_label_set_text(code_label_, c);
}

void PairScreen::set_host(const char *h) {
  if (h) lv_label_set_text(host_label_, h);
}

void PairScreen::btn_event(lv_event_t *e) {
  auto *self = static_cast<PairScreen *>(lv_event_get_user_data(e));
  if (self && self->cb_) self->cb_();
}

} // namespace cyd

#endif  // UNIT_TEST
