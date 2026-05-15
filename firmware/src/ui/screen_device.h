#pragma once

#include "app/app_config.h"

#ifndef UNIT_TEST

#include <lvgl.h>

#include "net/stats_types.h"

namespace cyd {

class ScreenDevice {
 public:
  void build(lv_obj_t *parent);
  void update(const Stats &s);   // daemon-health row only
  void set_device_info(const char *hostname, const char *ip, const char *version);
  void set_wifi_info(const char *ssid, int rssi_dbm);

 private:
  lv_obj_t *hostname_label_ = nullptr;
  lv_obj_t *ip_label_ = nullptr;
  lv_obj_t *wifi_label_ = nullptr;
  lv_obj_t *version_label_ = nullptr;
  lv_obj_t *daemon_label_ = nullptr;
};

}  // namespace cyd

#endif  // UNIT_TEST
