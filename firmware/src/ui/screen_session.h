#pragma once

#ifndef UNIT_TEST

#include <lvgl.h>

#include "net/stats_types.h"

namespace cyd {

class ScreenSession {
 public:
  void build(lv_obj_t *parent);
  void update(const Stats &s);

 private:
  lv_obj_t *arc_ = nullptr;
  lv_obj_t *pct_label_ = nullptr;
  lv_obj_t *resets_label_ = nullptr;
  lv_obj_t *model_a_ = nullptr;
  lv_obj_t *model_b_ = nullptr;
};

} // namespace cyd

#endif  // UNIT_TEST
