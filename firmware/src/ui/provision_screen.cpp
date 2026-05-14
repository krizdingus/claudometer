#include "ui/provision_screen.h"

#ifndef UNIT_TEST

#include "ui/theme.h"

namespace cyd {

void ProvisionScreen::build(lv_obj_t *parent) {
  theme::apply_screen_styles(parent);

  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Setup");
  lv_obj_set_style_text_color(title, theme::c(theme::accent), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

  auto *heading = lv_label_create(parent);
  lv_label_set_long_mode(heading, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(heading, 220);
  lv_label_set_text(heading, "Run claudometer\nadd-device");
  lv_obj_set_style_text_color(heading, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(heading, &lv_font_montserrat_16, 0);
  lv_obj_align(heading, LV_ALIGN_TOP_LEFT, 8, 90);

  auto *body = lv_label_create(parent);
  lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(body, 220);
  lv_label_set_text(body,
      "Connect this device\nvia USB and run\nthe command on your\ncomputer.");
  lv_obj_set_style_text_color(body, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(body, &lv_font_montserrat_14, 0);
  lv_obj_align(body, LV_ALIGN_TOP_LEFT, 8, 150);

  status_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(status_label_, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(status_label_, &lv_font_montserrat_14, 0);
  lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_label_set_text(status_label_, "Waiting for setup");
}

void ProvisionScreen::set_status(const char *msg) {
  if (status_label_) lv_label_set_text(status_label_, msg);
}

} // namespace cyd

#endif  // UNIT_TEST
