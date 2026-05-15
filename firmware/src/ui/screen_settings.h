#pragma once

#include "app/app_config.h"

#ifndef UNIT_TEST

#include <lvgl.h>
#include <string>

namespace cyd {

class Nvs;
class BrightnessController;
class Carousel;

class ScreenSettings {
 public:
  // Nvs persists theme; BrightnessController + Carousel apply & persist
  // their toggles live (no restart). All three pointers must outlive this screen.
  void build(lv_obj_t *parent, Nvs *nvs, BrightnessController *brightness, Carousel *carousel);

  // Persists the chosen theme and reboots so every screen renders in the new
  // palette. mode: 0 = dark, 1 = light.
  void apply_mode_and_restart(int mode);

  // Steps brightness one level up (+1) or down (-1) and persists. Live, no restart.
  void step_brightness(int direction);

  // Applies & persists the carousel toggle. Live, no restart.
  void apply_carousel(bool on);

 private:
  Nvs *nvs_ = nullptr;
  BrightnessController *brightness_ = nullptr;
  Carousel *carousel_ = nullptr;
  lv_obj_t *dark_pill_ = nullptr;
  lv_obj_t *light_pill_ = nullptr;
  lv_obj_t *minus_pill_ = nullptr;
  lv_obj_t *plus_pill_ = nullptr;
  lv_obj_t *level_label_ = nullptr;
  lv_obj_t *on_pill_ = nullptr;
  lv_obj_t *off_pill_ = nullptr;

  void apply_active_pill_styles();
};

}  // namespace cyd

#endif  // UNIT_TEST
