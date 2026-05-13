#pragma once

#ifndef UNIT_TEST

#include <lvgl.h>

#include "net/stats_types.h"

namespace cyd {

class ScreenChat {
 public:
  void build(lv_obj_t *parent);
  void update(const Stats &s);

 private:
  lv_obj_t *count_ = nullptr;
  lv_obj_t *cap_ = nullptr;
  lv_obj_t *bar_ = nullptr;
  lv_obj_t *empty_ = nullptr;
};

} // namespace cyd

#endif  // UNIT_TEST
