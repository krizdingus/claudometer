#pragma once

#ifndef UNIT_TEST

#include <lvgl.h>

#include "net/stats_types.h"

namespace cyd {

class ScreenModels {
 public:
  void build(lv_obj_t *parent);
  void update(const Stats &s);

  struct Row {
    lv_obj_t *label = nullptr;
    lv_obj_t *bar = nullptr;
    lv_obj_t *tokens = nullptr;
  };

 private:
  Row opus_, sonnet_, haiku_;
  lv_obj_t *total_ = nullptr;
  lv_obj_t *cost_ = nullptr;
};

} // namespace cyd

#endif  // UNIT_TEST
