#pragma once

#ifndef UNIT_TEST

#include <lvgl.h>

namespace cyd {
class ProvisionScreen {
 public:
  void build(lv_obj_t *parent);
  void set_ap_ssid(const char *ssid);
 private:
  lv_obj_t *ssid_label_ = nullptr;
};
} // namespace cyd

#endif  // UNIT_TEST
