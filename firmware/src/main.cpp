// firmware/src/main.cpp

#ifndef UNIT_TEST

#include <Arduino.h>

#include "app/app_loop.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  cyd::app_init();
}

void loop() {
  cyd::app_tick();
  delay(5);
}

#endif  // UNIT_TEST
