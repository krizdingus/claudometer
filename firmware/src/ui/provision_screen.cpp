#include "ui/provision_screen.h"

#ifndef UNIT_TEST

#include "ui/theme.h"

namespace cyd {

void ProvisionScreen::build(lv_obj_t *parent) {
  theme::apply_screen_styles(parent);
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Connect to WiFi");
  lv_obj_set_style_text_color(title, theme::c(theme::accent), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

  auto *step = lv_label_create(parent);
  lv_label_set_long_mode(step, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(step, 220);
  lv_label_set_text(step,
      "1. On your phone or laptop,\n   join the WiFi network:");
  lv_obj_set_style_text_color(step, theme::c(theme::fg), 0);
  lv_obj_set_style_text_font(step, &lv_font_montserrat_14, 0);
  lv_obj_align(step, LV_ALIGN_TOP_LEFT, 8, 80);

  ssid_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(ssid_label_, theme::c(theme::accent), 0);
  lv_obj_set_style_text_font(ssid_label_, &lv_font_montserrat_24, 0);
  lv_obj_align(ssid_label_, LV_ALIGN_TOP_MID, 0, 140);
  lv_label_set_text(ssid_label_, "ClaudeMonitor-XXXX");

  auto *finish = lv_label_create(parent);
  lv_label_set_long_mode(finish, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(finish, 220);
  lv_label_set_text(finish,
      "2. The captive portal opens.\n3. Pick your home WiFi,\n   enter the password.");
  lv_obj_set_style_text_color(finish, theme::c(theme::fg_muted), 0);
  lv_obj_set_style_text_font(finish, &lv_font_montserrat_14, 0);
  lv_obj_align(finish, LV_ALIGN_TOP_LEFT, 8, 200);
}

void ProvisionScreen::set_ap_ssid(const char *ssid) {
  lv_label_set_text(ssid_label_, ssid);
}

} // namespace cyd

#endif  // UNIT_TEST
