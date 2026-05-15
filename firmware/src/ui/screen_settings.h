#pragma once

#include "app/app_config.h"

#ifndef UNIT_TEST

#include <lvgl.h>
#include <string>

#include "net/stats_types.h"

namespace cyd {

class Nvs;
class BrightnessController;

class ScreenSettings {
 public:
  // Nvs persists theme changes; BrightnessController applies & persists the
  // Auto/Off toggle live (no restart). Both pointers must outlive this screen.
  void build(lv_obj_t *parent, Nvs *nvs, BrightnessController *brightness);
  void update(const Stats &s);
  void set_device_info(const char *hostname, const char *ip, const char *version);

  // Persists the chosen theme and reboots so every screen renders in the new
  // palette. mode: 0 = dark, 1 = light.
  void apply_mode_and_restart(int mode);

  // Steps brightness one level up (+1) or down (-1) and persists. Live, no restart.
  void step_brightness(int direction);

 private:
  Nvs *nvs_ = nullptr;
  BrightnessController *brightness_ = nullptr;
  lv_obj_t *dark_pill_ = nullptr;
  lv_obj_t *light_pill_ = nullptr;
  lv_obj_t *minus_pill_ = nullptr;
  lv_obj_t *plus_pill_ = nullptr;
  lv_obj_t *level_label_ = nullptr;
  lv_obj_t *hostname_label_ = nullptr;
  lv_obj_t *ip_label_ = nullptr;
  lv_obj_t *version_label_ = nullptr;
  lv_obj_t *daemon_label_ = nullptr;

  void apply_active_pill_styles();
};

}  // namespace cyd

#endif  // UNIT_TEST
