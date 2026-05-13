// firmware/src/main.cpp

#ifndef UNIT_TEST

#include <Arduino.h>

#include "hw/display.h"
#include "hw/touch.h"

void setup() {
  Serial.begin(115200);
  delay(200);

  auto &lcd = cyd::display();
  lcd.init();
  lcd.setRotation(0);
  lcd.setBrightness(200);
  lcd.fillScreen(0x0000);

  auto kind = cyd::touch().probe_and_init();
  lcd.setTextColor(0xFFFF, 0x0000);
  lcd.setCursor(10, 10);
  lcd.printf("touch: %s",
             kind == cyd::TouchKind::Capacitive ? "capacitive"
             : kind == cyd::TouchKind::Resistive ? "resistive" : "none");
}

void loop() {
  auto ev = cyd::touch().poll();
  if (ev.pressed) {
    Serial.printf("touch @ (%d, %d)\n", ev.x, ev.y);
    cyd::display().fillCircle(ev.x, ev.y, 4, 0xFFFF);
  }
  delay(20);
}

#endif  // UNIT_TEST
