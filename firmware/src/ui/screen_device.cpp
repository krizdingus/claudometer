#include "ui/screen_device.h"

#ifndef UNIT_TEST

#include <Arduino.h>
#include <stdio.h>

#include "ui/theme.h"

namespace cyd {

void ScreenDevice::build(lv_obj_t *parent) {
  auto *title = lv_label_create(parent);
  lv_label_set_text(title, "Device");
  lv_obj_set_style_text_color(title, theme::fg(), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

  hostname_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(hostname_label_, theme::fg(), 0);
  lv_obj_set_style_text_font(hostname_label_, &lv_font_montserrat_14, 0);
  lv_obj_align(hostname_label_, LV_ALIGN_TOP_LEFT, 4, 22);
  lv_label_set_text(hostname_label_, "-");

  ip_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(ip_label_, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(ip_label_, &lv_font_montserrat_12, 0);
  lv_obj_align(ip_label_, LV_ALIGN_TOP_LEFT, 4, 44);
  lv_label_set_text(ip_label_, "-");

  wifi_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(wifi_label_, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(wifi_label_, &lv_font_montserrat_12, 0);
  lv_obj_align(wifi_label_, LV_ALIGN_TOP_LEFT, 4, 64);
  lv_label_set_text(wifi_label_, "-");

  version_label_ = lv_label_create(parent);
  lv_obj_set_style_text_color(version_label_, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(version_label_, &lv_font_montserrat_12, 0);
  lv_obj_align(version_label_, LV_ALIGN_TOP_LEFT, 4, 84);
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
  lv_obj_align(daemon_label_, LV_ALIGN_TOP_LEFT, 4, 104);
  lv_label_set_text(daemon_label_, "daemon -");

  auto *divider = lv_obj_create(parent);
  lv_obj_set_size(divider, 224, 1);
  lv_obj_align(divider, LV_ALIGN_TOP_LEFT, 4, 148);
  lv_obj_set_style_bg_color(divider, theme::bar_bg(), 0);
  lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(divider, 0, 0);
  lv_obj_set_style_radius(divider, 0, 0);

  auto *repo = lv_label_create(parent);
  lv_obj_set_style_text_color(repo, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(repo, &lv_font_montserrat_12, 0);
  lv_obj_align(repo, LV_ALIGN_TOP_LEFT, 4, 158);
  lv_label_set_text(repo, "github.com/krizdingus/claudometer");

  auto *site = lv_label_create(parent);
  lv_obj_set_style_text_color(site, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(site, &lv_font_montserrat_12, 0);
  lv_obj_align(site, LV_ALIGN_TOP_LEFT, 4, 178);
  lv_label_set_text(site, "krizdingus.com");

  auto *author = lv_label_create(parent);
  lv_obj_set_style_text_color(author, theme::fg_muted(), 0);
  lv_obj_set_style_text_font(author, &lv_font_montserrat_12, 0);
  lv_obj_align(author, LV_ALIGN_TOP_LEFT, 4, 198);
  lv_label_set_text(author, "2026 All Krizdingus fault");
}

void ScreenDevice::update(const Stats &s) {
  if (!daemon_label_) return;
  if (s.stale) {
    lv_label_set_text(daemon_label_, "daemon - stale");
    lv_obj_set_style_text_color(daemon_label_, theme::warn(), 0);
  } else {
    lv_label_set_text(daemon_label_, "daemon - ok");
    lv_obj_set_style_text_color(daemon_label_, theme::ok(), 0);
  }
}

void ScreenDevice::set_device_info(const char *hostname, const char *ip, const char *version) {
  if (hostname && hostname_label_) lv_label_set_text(hostname_label_, hostname);
  if (ip && ip_label_)             lv_label_set_text(ip_label_, ip);
  if (version && version_label_) {
    char vbuf[32];
    snprintf(vbuf, sizeof(vbuf), "v%s", version);
    lv_label_set_text(version_label_, vbuf);
  }
}

void ScreenDevice::set_wifi_info(const char *ssid, int rssi_dbm) {
  if (!wifi_label_) return;
  if (!ssid || ssid[0] == '\0') {
    lv_label_set_text(wifi_label_, "-");
    return;
  }
  char buf[64];
  snprintf(buf, sizeof(buf), "%s  %d dBm", ssid, rssi_dbm);
  lv_label_set_text(wifi_label_, buf);
}

}  // namespace cyd

#endif  // UNIT_TEST
