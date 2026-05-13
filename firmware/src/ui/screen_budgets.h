#pragma once

#ifndef UNIT_TEST

#include <lvgl.h>

#include "net/stats_types.h"

namespace cyd {

class ScreenBudgets {
 public:
  void build(lv_obj_t *parent);
  void update(const Stats &s);

  struct Row {
    lv_obj_t *label = nullptr;
    lv_obj_t *bar = nullptr;
    lv_obj_t *pct = nullptr;
  };

 private:
  Row code_all_, code_opus_, chat_;
  lv_obj_t *plan_ = nullptr;
  lv_obj_t *resets_ = nullptr;
  lv_obj_t *warn_ = nullptr;
};

} // namespace cyd

#endif  // UNIT_TEST
