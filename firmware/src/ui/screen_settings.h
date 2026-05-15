#pragma once

#include "app/app_config.h"

#ifndef UNIT_TEST

#include <lvgl.h>
#include <string>

#include "net/stats_types.h"

namespace cyd {

class Nvs;

class ScreenSettings {
 public:
  // The Nvs pointer is used to persist theme changes. Must outlive this screen.
  void build(lv_obj_t *parent, Nvs *nvs);
  void update(const Stats &s);              // mostly for health-derived rows
  void set_device_info(const char *hostname, const char *ip, const char *version);

  // Called by app_loop when a tap is detected on this tile.
  // Returns true if handled (theme toggle hit). Coordinates are screen-relative.
  bool on_tap(int x, int y);

 private:
  Nvs *nvs_ = nullptr;
  lv_obj_t *dark_pill_ = nullptr;
  lv_obj_t *light_pill_ = nullptr;
  lv_obj_t *hostname_label_ = nullptr;
  lv_obj_t *ip_label_ = nullptr;
  lv_obj_t *version_label_ = nullptr;
  lv_obj_t *daemon_label_ = nullptr;

  void apply_active_pill_styles();
};

}  // namespace cyd

#endif  // UNIT_TEST
