// firmware/src/main.cpp

#ifndef UNIT_TEST

#include <Arduino.h>
#include <lvgl.h>

#include "hw/display.h"
#include "hw/touch.h"
#include "ui/lvgl_glue.h"

void setup() {
  Serial.begin(115200);
  cyd::display().init();
  cyd::display().setRotation(0);
  cyd::display().setBrightness(200);
  cyd::display().fillScreen(0x0000);
  cyd::touch().probe_and_init();
  cyd::lvgl_init();

  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0E0E10), 0);
  lv_obj_t *label = lv_label_create(scr);
  lv_label_set_text(label, "LVGL OK\nclaude monitor");
  lv_obj_set_style_text_color(label, lv_color_hex(0xD97757), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

void loop() {
  cyd::lvgl_tick();
  delay(5);
}

#endif  // UNIT_TEST
