// firmware/src/main.cpp

#ifndef UNIT_TEST

#include <Arduino.h>

#include "hw/display.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("cydmonitor firmware booting");

  auto &lcd = cyd::display();
  lcd.init();
  lcd.setRotation(0);
  lcd.setBrightness(200);

  lcd.fillScreen(0x0000);                // black
  lcd.fillRect(0, 0, 240, 80, 0xF800);   // red top
  lcd.fillRect(0, 80, 240, 80, 0x07E0);  // green middle
  lcd.fillRect(0, 160, 240, 80, 0x001F); // blue
  lcd.fillRect(0, 240, 240, 80, 0xFFE0); // yellow bottom
  lcd.setTextColor(0xFFFF, 0x0000);
  lcd.setCursor(20, 300);
  lcd.printf("CYD display OK");
}

void loop() {
  delay(1000);
}

#endif  // UNIT_TEST
