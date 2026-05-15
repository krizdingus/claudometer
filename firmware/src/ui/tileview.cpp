#include "ui/tileview.h"

#ifndef UNIT_TEST

#include "ui/theme.h"

namespace cyd {

void Tileview::attach(lv_obj_t *parent) {
  tv_ = lv_tileview_create(parent);
  lv_obj_set_size(tv_, 240, 320 - kStatusBarHeight - kFooterHeight);
  lv_obj_align(tv_, LV_ALIGN_TOP_MID, 0, kStatusBarHeight);
  lv_obj_set_style_bg_color(tv_, theme::bg(), 0);
  lv_obj_set_style_border_width(tv_, 0, 0);
  lv_obj_set_scrollbar_mode(tv_, LV_SCROLLBAR_MODE_OFF);

  for (int i = 0; i < SCR_COUNT; ++i) {
    tiles_[i] = lv_tileview_add_tile(tv_, i, 0, LV_DIR_HOR);
    theme::apply_screen_styles(tiles_[i]);
  }
  lv_obj_set_user_data(tv_, this);
  lv_obj_add_event_cb(tv_, &Tileview::event_cb, LV_EVENT_VALUE_CHANGED, this);
}

lv_obj_t *Tileview::tile(Screen s) { return tiles_[s]; }

Screen Tileview::active() const {
  lv_obj_t *cur = lv_tileview_get_tile_active(tv_);
  for (int i = 0; i < SCR_COUNT; ++i) {
    if (tiles_[i] == cur) return static_cast<Screen>(i);
  }
  return SCR_SESSION;
}

void Tileview::set_active(Screen s, lv_anim_enable_t anim) {
  lv_tileview_set_tile(tv_, tiles_[s], anim);
}

uint8_t Tileview::neighbor_mask() const {
  int a = active();
  uint8_t mask = 1 << a;
  if (a > 0)              mask |= 1 << (a - 1);
  if (a < SCR_COUNT - 1)  mask |= 1 << (a + 1);
  return mask;
}

void Tileview::event_cb(lv_event_t *e) {
  auto *self = static_cast<Tileview *>(lv_event_get_user_data(e));
  if (self && self->change_cb_) self->change_cb_(self->active());
}

} // namespace cyd

#endif  // UNIT_TEST
