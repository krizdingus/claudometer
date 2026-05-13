#pragma once

#include "app/app_config.h"

#ifndef UNIT_TEST

#include <lvgl.h>

namespace cyd {

// Owns one lv_tileview with 6 horizontal tiles in a vertical strip. Each
// concrete screen module (screen_session, …) receives its tile root and
// fills it in. The tileview itself is sized to (240, 320 - chrome).
class Tileview {
 public:
  void attach(lv_obj_t *parent);
  lv_obj_t *tile(Screen s);
  Screen active() const;
  void set_active(Screen s, lv_anim_enable_t anim = LV_ANIM_ON);

  // Returns the bitmask of screens within ±1 of the active one (the screens
  // the firmware will request from the daemon).
  uint8_t neighbor_mask() const;

  // Called on every active-screen change to keep chrome in sync.
  using ChangeCb = void (*)(Screen);
  void on_change(ChangeCb cb) { change_cb_ = cb; }

 private:
  lv_obj_t *tv_ = nullptr;
  lv_obj_t *tiles_[SCR_COUNT] = {nullptr};
  ChangeCb change_cb_ = nullptr;

  static void event_cb(lv_event_t *e);
};

} // namespace cyd

#endif  // UNIT_TEST
