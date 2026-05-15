#include "ui/screen_settings.h"

#ifndef UNIT_TEST

#include <stdio.h>

#include "hw/nvs.h"
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

lv_obj_t *make_pill(lv_obj_t *parent, const char *text, int x, int y) {
  auto *pill = lv_obj_create(parent);
  lv_obj_set_size(pill, 96, 28);
  lv_obj_set_pos(pill, x, y);
  lv_obj_set_style_radius(pill, 14, 0);
  lv_obj_set_style_border_width(pill, 1, 0);
  lv_obj_set_style_border_color(pill, theme::fg_muted(), 0);
  lv_obj_set_style_bg_opa(pill, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(pill, 0, 0);
  lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);

  auto *label = lv_label_create(pill);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, theme::fg(), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_center(label);
  lv_obj_set_user_data(pill, label);

  return pill;
}

}  // namespace

void ScreenSettings::build(lv_obj_t *parent, Nvs *nvs) {
  nvs_ = nvs;

  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Settings");
  lv_obj_set_style_text_color(title, theme::fg(), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

  hairline(parent, 24);

  auto *theme_label = lv_label_create(parent);
  lv_label_set_text(theme_label, "Theme");
  lv_obj_set_style_text_color(theme_label, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(theme_label, &lv_font_montserrat_12, 0);
  lv_obj_align(theme_label, LV_ALIGN_TOP_LEFT, 4, 36);

  dark_pill_  = make_pill(parent, "Dark",  8,   58);
  light_pill_ = make_pill(parent, "Light", 128, 58);
  apply_active_pill_styles();

  hairline(parent, 100);

  auto *device_label = lv_label_create(parent);
  lv_label_set_text(device_label, "Device");
  lv_obj_set_style_text_color(device_label, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(device_label, &lv_font_montserrat_12, 0);
  lv_obj_align(device_label, LV_ALIGN_TOP_LEFT, 4, 110);

  hostname_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(hostname_label_, theme::fg(), 0);
  lv_obj_set_style_text_font(hostname_label_, &lv_font_montserrat_14, 0);
  lv_obj_align(hostname_label_, LV_ALIGN_TOP_LEFT, 4, 128);
  lv_label_set_text(hostname_label_, "—");

  ip_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(ip_label_, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(ip_label_, &lv_font_montserrat_12, 0);
  lv_obj_align(ip_label_, LV_ALIGN_TOP_LEFT, 4, 148);
  lv_label_set_text(ip_label_, "—");

  version_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(version_label_, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(version_label_, &lv_font_montserrat_12, 0);
  lv_obj_align(version_label_, LV_ALIGN_TOP_LEFT, 4, 168);
#ifdef FIRMWARE_VERSION
  char vbuf[32];
  snprintf(vbuf, sizeof(vbuf), "v%s", FIRMWARE_VERSION);
  lv_label_set_text(version_label_, vbuf);
#else
  lv_label_set_text(version_label_, "v?");
#endif

  daemon_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(daemon_label_, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(daemon_label_, &lv_font_montserrat_12, 0);
  lv_obj_align(daemon_label_, LV_ALIGN_TOP_LEFT, 4, 188);
  lv_label_set_text(daemon_label_, "daemon —");
}

void ScreenSettings::apply_active_pill_styles() {
  bool is_dark = theme::get_mode() == theme::Mode::Dark;

  auto active_styling = [](lv_obj_t *pill) {
    lv_obj_set_style_bg_color(pill, theme::accent(), 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(pill, theme::accent(), 0);
    auto *label = static_cast<lv_obj_t *>(lv_obj_get_user_data(pill));
    lv_obj_set_style_text_color(label, theme::bg(), 0);
  };
  auto inactive_styling = [](lv_obj_t *pill) {
    lv_obj_set_style_bg_opa(pill, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(pill, theme::fg_muted(), 0);
    auto *label = static_cast<lv_obj_t *>(lv_obj_get_user_data(pill));
    lv_obj_set_style_text_color(label, theme::fg(), 0);
  };

  if (is_dark) {
    active_styling(dark_pill_);
    inactive_styling(light_pill_);
  } else {
    active_styling(light_pill_);
    inactive_styling(dark_pill_);
  }
}

void ScreenSettings::set_device_info(const char *hostname, const char *ip, const char *version) {
  if (hostname) lv_label_set_text(hostname_label_, hostname);
  if (ip) lv_label_set_text(ip_label_, ip);
  if (version) {
    char vbuf[32];
    snprintf(vbuf, sizeof(vbuf), "v%s", version);
    lv_label_set_text(version_label_, vbuf);
  }
}

void ScreenSettings::update(const Stats &s) {
  if (s.stale) {
    lv_label_set_text(daemon_label_, "daemon — stale");
    lv_obj_set_style_text_color(daemon_label_, theme::warn(), 0);
  } else {
    lv_label_set_text(daemon_label_, "daemon — ok");
    lv_obj_set_style_text_color(daemon_label_, theme::ok(), 0);
  }
}

bool ScreenSettings::on_tap(int x, int y) {
  auto hit = [&](lv_obj_t *o) {
    if (!o) return false;
    int ox = lv_obj_get_x(o);
    int oy = lv_obj_get_y(o);
    int ow = lv_obj_get_width(o);
    int oh = lv_obj_get_height(o);
    return x >= ox && x < ox + ow && y >= oy && y < oy + oh;
  };

  if (hit(dark_pill_)) {
    theme::set_mode(theme::Mode::Dark);
    if (nvs_) nvs_->save_theme(0);
    apply_active_pill_styles();
    return true;
  }
  if (hit(light_pill_)) {
    theme::set_mode(theme::Mode::Light);
    if (nvs_) nvs_->save_theme(1);
    apply_active_pill_styles();
    return true;
  }
  return false;
}

}  // namespace cyd

#endif  // UNIT_TEST
