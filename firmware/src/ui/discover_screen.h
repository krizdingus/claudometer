#pragma once

#ifndef UNIT_TEST

#include <lvgl.h>

namespace cyd {
class DiscoverScreen {
 public:
  void build(lv_obj_t *parent);
  void set_status(const char *status);
 private:
  lv_obj_t *status_ = nullptr;
};
} // namespace cyd

#endif  // UNIT_TEST
