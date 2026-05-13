#include "hw/touch.h"

#ifndef UNIT_TEST

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include "hw/pins.h"

namespace cyd {

namespace {
SPIClass xpt_spi(VSPI);

bool ft6336_present() {
  Wire.begin(pins::FT_SDA, pins::FT_SCL, 400000);
  Wire.beginTransmission(pins::FT_ADDR);
  return Wire.endTransmission() == 0;
}

void init_xpt2046() {
  pinMode(pins::XPT_CS, OUTPUT);
  digitalWrite(pins::XPT_CS, HIGH);
  xpt_spi.begin(pins::XPT_SCLK, pins::XPT_MISO, pins::XPT_MOSI, pins::XPT_CS);
  xpt_spi.setFrequency(2000000);
}

TouchEvent read_ft6336() {
  TouchEvent ev;
  Wire.beginTransmission(pins::FT_ADDR);
  Wire.write(0x02);                  // num touches register
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
  if (digitalRead(pins::XPT_IRQ) == HIGH) return ev;  // IRQ low = pressed
  digitalWrite(pins::XPT_CS, LOW);
  xpt_spi.transfer(0xD0);                              // X channel
  uint16_t x = (xpt_spi.transfer(0) << 8 | xpt_spi.transfer(0)) >> 3;
  xpt_spi.transfer(0x90);                              // Y channel
  uint16_t y = (xpt_spi.transfer(0) << 8 | xpt_spi.transfer(0)) >> 3;
  digitalWrite(pins::XPT_CS, HIGH);
  if (x == 0 && y == 0) return ev;
  ev.pressed = true;
  // Map raw 12-bit to 240x320. Calibration is loaded from NVS in production;
  // this fallback assumes a roughly linear mapping.
  ev.x = map(x, 200, 3900, 0, 240);
  ev.y = map(y, 240, 3800, 0, 320);
  return ev;
}
}  // namespace

TouchKind Touch::probe_and_init() {
  if (ft6336_present()) {
    kind_ = TouchKind::Capacitive;
    Serial.println("touch: FT6336 (capacitive) detected");
  } else {
    init_xpt2046();
    kind_ = TouchKind::Resistive;
    Serial.println("touch: XPT2046 (resistive) fallback");
  }
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

} // namespace cyd

#endif  // UNIT_TEST
