#include "hw/touch.h"

#ifndef UNIT_TEST

#include <Arduino.h>
#include <Wire.h>

#include "hw/display.h"
#include "hw/pins.h"

namespace cyd {

namespace {

bool ft6336_present() {
  Wire.begin(pins::FT_SDA, pins::FT_SCL, 400000);
  Wire.beginTransmission(pins::FT_ADDR);
  bool ok = Wire.endTransmission() == 0;
  if (!ok) Wire.end();
  return ok;
}

TouchEvent read_ft6336() {
  TouchEvent ev;
  Wire.beginTransmission(pins::FT_ADDR);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return ev;
  Wire.requestFrom(pins::FT_ADDR, (uint8_t)5);
  if (Wire.available() < 5) return ev;
  uint8_t n = Wire.read();
  uint8_t xh = Wire.read();
  uint8_t xl = Wire.read();
  uint8_t yh = Wire.read();
  uint8_t yl = Wire.read();
  if (n == 0) return ev;
  ev.pressed = true;
  ev.x = ((xh & 0x0F) << 8) | xl;
  ev.y = ((yh & 0x0F) << 8) | yl;
  return ev;
}

TouchEvent read_xpt2046() {
  TouchEvent ev;
  int32_t x = 0, y = 0;
  if (display().getTouch(&x, &y) > 0) {
    ev.pressed = true;
    ev.x = x;
    ev.y = y;
    int32_t rx = 0, ry = 0;
    display().getTouchRaw(&rx, &ry);
    static uint32_t last_log = 0;
    if (millis() - last_log > 200) {
      Serial.printf("touch raw=(%d,%d) mapped=(%d,%d)\n", rx, ry, x, y);
      last_log = millis();
    }
  }
  return ev;
}

}  // namespace

TouchKind Touch::probe_and_init() {
  // FT6336 probe is intentionally skipped — calling Wire.begin(33, 32, ...)
  // here reconfigures pin 33 as I²C SDA, undoing LovyanGFX's XPT_CS setup
  // from display().init() and leaving touch dead. The known-good path on
  // ESP32-2432S028R uses XPT2046 over the shared HSPI bus, configured by
  // LovyanGFX. To re-enable capacitive support, move the probe to run
  // BEFORE display().init() so it doesn't clobber pin state.
  kind_ = TouchKind::Resistive;
  Serial.println("touch: XPT2046 (resistive) — FT6336 probe skipped");
  return kind_;
}

TouchEvent Touch::poll() {
  if (kind_ == TouchKind::Capacitive) return read_ft6336();
  if (kind_ == TouchKind::Resistive) return read_xpt2046();
  return {};
}

Touch &touch() {
  static Touch t;
  return t;
}

}  // namespace cyd

#endif  // UNIT_TEST
