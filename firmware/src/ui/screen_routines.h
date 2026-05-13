#pragma once

#ifndef UNIT_TEST

#include <lvgl.h>

#include "net/stats_types.h"

namespace cyd {

class ScreenRoutines {
 public:
  void build(lv_obj_t *parent);
  void update(const Stats &s);

 private:
  static constexpr int kMaxRows = 5;
  struct Row {
    lv_obj_t *name = nullptr;
    lv_obj_t *pill = nullptr;
    lv_obj_t *when = nullptr;
  };
  Row rows_[kMaxRows];
  lv_obj_t *empty_ = nullptr;
};

} // namespace cyd

#endif  // UNIT_TEST
