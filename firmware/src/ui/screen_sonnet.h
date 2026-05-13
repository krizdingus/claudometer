#pragma once

#ifndef UNIT_TEST

#include <lvgl.h>

#include "net/stats_types.h"

namespace cyd {

class ScreenSonnet {
 public:
  void build(lv_obj_t *parent);
  void update(const Stats &s);

 private:
  lv_obj_t *bar_ = nullptr;
  lv_obj_t *pct_ = nullptr;
  lv_obj_t *amount_ = nullptr;
  lv_obj_t *pace_ = nullptr;
};

} // namespace cyd

#endif  // UNIT_TEST
