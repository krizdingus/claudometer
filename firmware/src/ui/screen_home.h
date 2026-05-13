#pragma once

#ifndef UNIT_TEST

#include <lvgl.h>

#include "net/stats_types.h"

namespace cyd {

// Front-page digest. Shows the two stats people actually want at a glance:
// the current 5-hour session block and the all-models weekly window, plus a
// quick "today" total. Other screens drill into each in detail.
class ScreenHome {
 public:
  void build(lv_obj_t *parent);
  void update(const Stats &s);

 private:
  lv_obj_t *plan_pill_ = nullptr;
  lv_obj_t *plan_pill_label_ = nullptr;

  lv_obj_t *session_pct_ = nullptr;
  lv_obj_t *session_bar_ = nullptr;
  lv_obj_t *session_meta_ = nullptr;

  lv_obj_t *week_pct_ = nullptr;
  lv_obj_t *week_bar_ = nullptr;
  lv_obj_t *week_meta_ = nullptr;

  lv_obj_t *today_line_ = nullptr;
};

} // namespace cyd

#endif  // UNIT_TEST
