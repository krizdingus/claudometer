#pragma once

#ifndef UNIT_TEST

#include <lvgl.h>

namespace cyd {
class PairScreen {
 public:
  using ConfirmCb = void (*)();
  void build(lv_obj_t *parent);
  void set_code(const char *code);
  void set_host(const char *host);
  void on_confirm(ConfirmCb cb) { cb_ = cb; }
 private:
  lv_obj_t *code_label_ = nullptr;
  lv_obj_t *host_label_ = nullptr;
  lv_obj_t *button_ = nullptr;
  ConfirmCb cb_ = nullptr;
  static void btn_event(lv_event_t *e);
};
} // namespace cyd

#endif  // UNIT_TEST
