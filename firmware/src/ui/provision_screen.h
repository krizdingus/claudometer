#pragma once

#ifndef UNIT_TEST

#include <lvgl.h>

namespace cyd {
class ProvisionScreen {
 public:
  void build(lv_obj_t *parent);
  void set_status(const char *msg);
 private:
  lv_obj_t *status_label_ = nullptr;
};
} // namespace cyd

#endif  // UNIT_TEST
