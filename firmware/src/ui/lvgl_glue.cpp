#include "ui/lvgl_glue.h"

#ifndef UNIT_TEST

#include <Arduino.h>
#include <lvgl.h>

#include "hw/display.h"
#include "hw/touch.h"

namespace cyd {

namespace {

constexpr uint32_t kScreenW = 240;
constexpr uint32_t kScreenH = 320;
constexpr uint32_t kBufRows = 40;

// Two partial draw buffers (double-buffered). 240 * 40 * 2 bytes = 19200 each.
static lv_color_t buf_a[kScreenW * kBufRows];
static lv_color_t buf_b[kScreenW * kBufRows];

void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  auto w = area->x2 - area->x1 + 1;
  auto h = area->y2 - area->y1 + 1;
  display().pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t *)px_map);
  lv_display_flush_ready(disp);
}

void touch_read_cb(lv_indev_t *, lv_indev_data_t *data) {
  auto ev = touch().poll();
  data->state = ev.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  if (ev.pressed) {
    data->point.x = ev.x;
    data->point.y = ev.y;
  }
}

} // namespace

void lvgl_init() {
  lv_init();

  auto *disp = lv_display_create(kScreenW, kScreenH);
  lv_display_set_buffers(disp, buf_a, buf_b, sizeof(buf_a),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, flush_cb);

  auto *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_read_cb);
}

void lvgl_tick() {
  lv_timer_handler();
}

} // namespace cyd

#endif  // UNIT_TEST
