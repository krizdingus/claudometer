#include "ui/screen_settings.h"

#ifndef UNIT_TEST

#include <Arduino.h>
#include <stdio.h>

#include "app/app_config.h"
#include "app/brightness_controller.h"
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

lv_obj_t *make_pill(lv_obj_t *parent, const char *text, int x, int y, int w = 96) {
  auto *pill = lv_obj_create(parent);
  lv_obj_set_size(pill, w, 28);
  lv_obj_set_pos(pill, x, y);
  lv_obj_set_style_radius(pill, 14, 0);
  lv_obj_set_style_border_width(pill, 1, 0);
  lv_obj_set_style_border_color(pill, theme::fg_muted(), 0);
  lv_obj_set_style_bg_opa(pill, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(pill, 0, 0);
  lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(pill, LV_OBJ_FLAG_CLICKABLE);

  auto *label = lv_label_create(pill);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, theme::fg(), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_center(label);
  lv_obj_set_user_data(pill, label);
  // Don't let the label intercept clicks — let them bubble to the pill.
  lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);

  return pill;
}

// Both pills must be created via make_pill() (label is stored as user_data).
void apply_pill_pair_styles(lv_obj_t *active, lv_obj_t *inactive) {
  // Active pill: filled with accent.
  lv_obj_set_style_bg_color(active, theme::accent(), 0);
  lv_obj_set_style_bg_opa(active, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(active, theme::accent(), 0);
  auto *active_label = static_cast<lv_obj_t *>(lv_obj_get_user_data(active));
  lv_obj_set_style_text_color(active_label, theme::bg(), 0);

  // Inactive pill: transparent with muted border.
  lv_obj_set_style_bg_opa(inactive, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(inactive, theme::fg_muted(), 0);
  auto *inactive_label = static_cast<lv_obj_t *>(lv_obj_get_user_data(inactive));
  lv_obj_set_style_text_color(inactive_label, theme::fg(), 0);
}

static void on_dark_clicked(lv_event_t *e) {
  auto *self = static_cast<ScreenSettings *>(lv_event_get_user_data(e));
  if (self) self->apply_mode_and_restart(0);
}

static void on_light_clicked(lv_event_t *e) {
  auto *self = static_cast<ScreenSettings *>(lv_event_get_user_data(e));
  if (self) self->apply_mode_and_restart(1);
}

int duty_to_idx(uint8_t duty) {
  if (duty <= kBrightnessLow) return 0;
  if (duty <= kBrightnessMed) return 1;
  if (duty <= kBrightnessHigh) return 2;
  return 3;
}

uint8_t idx_to_duty(int idx) {
  switch (idx) {
    case 0:  return kBrightnessLow;
    case 1:  return kBrightnessMed;
    case 2:  return kBrightnessHigh;
    case 3:  return kBrightnessMax;
    default: return kBrightnessDefault;
  }
}

const char *idx_to_name(int idx) {
  switch (idx) {
    case 0:  return "Low";
    case 1:  return "Med";
    case 2:  return "High";
    case 3:  return "Max";
    default: return "?";
  }
}

static void on_minus_clicked(lv_event_t *e) {
  auto *self = static_cast<ScreenSettings *>(lv_event_get_user_data(e));
  if (self) self->step_brightness(-1);
}

static void on_plus_clicked(lv_event_t *e) {
  auto *self = static_cast<ScreenSettings *>(lv_event_get_user_data(e));
  if (self) self->step_brightness(+1);
}

}  // namespace

void ScreenSettings::build(lv_obj_t *parent, Nvs *nvs, BrightnessController *brightness) {
  nvs_ = nvs;
  brightness_ = brightness;

  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Settings");
  lv_obj_set_style_text_color(title, theme::fg(), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

  hairline(parent, 24);

  // Theme
  auto *theme_label = lv_label_create(parent);
  lv_label_set_text(theme_label, "Theme");
  lv_obj_set_style_text_color(theme_label, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(theme_label, &lv_font_montserrat_12, 0);
  lv_obj_align(theme_label, LV_ALIGN_TOP_LEFT, 4, 36);

  dark_pill_  = make_pill(parent, "Dark",  8,   58);
  light_pill_ = make_pill(parent, "Light", 120, 58);
  lv_obj_add_event_cb(dark_pill_,  on_dark_clicked,  LV_EVENT_CLICKED, this);
  lv_obj_add_event_cb(light_pill_, on_light_clicked, LV_EVENT_CLICKED, this);

  hairline(parent, 92);

  // Brightness
  auto *bright_label = lv_label_create(parent);
  lv_label_set_text(bright_label, "Brightness");
  lv_obj_set_style_text_color(bright_label, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(bright_label, &lv_font_montserrat_12, 0);
  lv_obj_align(bright_label, LV_ALIGN_TOP_LEFT, 4, 102);

  minus_pill_ = make_pill(parent, "-", 8,   124, 48);
  plus_pill_  = make_pill(parent, "+", 168, 124, 48);
  lv_obj_add_event_cb(minus_pill_, on_minus_clicked, LV_EVENT_CLICKED, this);
  lv_obj_add_event_cb(plus_pill_,  on_plus_clicked,  LV_EVENT_CLICKED, this);

  level_label_ = lv_label_create(parent);
  lv_label_set_text(level_label_, "High");
  lv_obj_set_style_text_color(level_label_, theme::fg(), 0);
  lv_obj_set_style_text_font(level_label_, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(level_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(level_label_, 56, 124 + 5);
  lv_obj_set_size(level_label_, 112, 18);

  apply_active_pill_styles();

  hairline(parent, 158);

  // Device
  auto *device_label = lv_label_create(parent);
  lv_label_set_text(device_label, "Device");
  lv_obj_set_style_text_color(device_label, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(device_label, &lv_font_montserrat_12, 0);
  lv_obj_align(device_label, LV_ALIGN_TOP_LEFT, 4, 168);

  hostname_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(hostname_label_, theme::fg(), 0);
  lv_obj_set_style_text_font(hostname_label_, &lv_font_montserrat_14, 0);
  lv_obj_align(hostname_label_, LV_ALIGN_TOP_LEFT, 4, 186);
  lv_label_set_text(hostname_label_, "-");

  ip_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(ip_label_, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(ip_label_, &lv_font_montserrat_12, 0);
  lv_obj_align(ip_label_, LV_ALIGN_TOP_LEFT, 4, 204);
  lv_label_set_text(ip_label_, "-");

  version_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(version_label_, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(version_label_, &lv_font_montserrat_12, 0);
  lv_obj_align(version_label_, LV_ALIGN_TOP_LEFT, 4, 220);
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
  lv_obj_align(daemon_label_, LV_ALIGN_TOP_LEFT, 4, 236);
  lv_label_set_text(daemon_label_, "daemon -");
}

void ScreenSettings::apply_active_pill_styles() {
  bool is_dark = theme::get_mode() == theme::Mode::Dark;
  if (is_dark) apply_pill_pair_styles(dark_pill_, light_pill_);
  else         apply_pill_pair_styles(light_pill_, dark_pill_);

  if (minus_pill_ && plus_pill_ && level_label_ && brightness_) {
    int idx = duty_to_idx(brightness_->level());
    lv_label_set_text(level_label_, idx_to_name(idx));
    // +/- pills always render as inactive-styled (always tappable, never "selected").
    auto inactive = [](lv_obj_t *pill) {
      lv_obj_set_style_bg_opa(pill, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_color(pill, theme::fg_muted(), 0);
      auto *label = static_cast<lv_obj_t *>(lv_obj_get_user_data(pill));
      lv_obj_set_style_text_color(label, theme::fg(), 0);
    };
    inactive(minus_pill_);
    inactive(plus_pill_);
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
    lv_label_set_text(daemon_label_, "daemon - stale");
    lv_obj_set_style_text_color(daemon_label_, theme::warn(), 0);
  } else {
    lv_label_set_text(daemon_label_, "daemon - ok");
    lv_obj_set_style_text_color(daemon_label_, theme::ok(), 0);
  }
}

void ScreenSettings::apply_mode_and_restart(int mode) {
  if (nvs_) nvs_->save_theme(mode);
  Serial.printf("theme: switching to %s, restarting\n", mode == 1 ? "light" : "dark");
  delay(150);
  ESP.restart();
}

void ScreenSettings::step_brightness(int direction) {
  if (!brightness_) return;
  int idx = duty_to_idx(brightness_->level()) + direction;
  if (idx < 0) idx = 0;
  if (idx > 3) idx = 3;
  brightness_->set_level(idx_to_duty(idx));
  apply_active_pill_styles();  // updates level label + restyles pills
}

}  // namespace cyd

#endif  // UNIT_TEST
