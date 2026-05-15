#pragma once

#ifndef UNIT_TEST

#include <lvgl.h>

#include "app/app_config.h"

namespace cyd {

class Chrome {
 public:
  // Attach permanent status bar (top) and pip footer (bottom) to a parent.
  // The parent is typically the active screen or a layer above tileview.
  void attach(lv_obj_t *parent);

  // health: 0 = ok (green dot), 1 = stale (yellow), 2 = offline (red).
  void set_health(int health);
  void set_clock(const char *hhmm);
  void set_plan(const char *pretty_plan);  // e.g. "MAX 20x", "PRO". Empty/null hides the pill.
  void set_active_screen(int index);

 private:
  lv_obj_t *dot_ = nullptr;
  lv_obj_t *clock_ = nullptr;
  lv_obj_t *plan_pill_ = nullptr;
  lv_obj_t *plan_pill_label_ = nullptr;
  lv_obj_t *pips_[SCR_COUNT] = {nullptr};
};

} // namespace cyd

#endif  // UNIT_TEST
