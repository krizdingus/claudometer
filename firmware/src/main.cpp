// firmware/src/main.cpp

#ifndef UNIT_TEST

#include <Arduino.h>

#include "app/app_loop.h"

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("[boot] cydmonitor firmware booting");
  Serial.flush();
  cyd::app_init();
  Serial.println("[boot] app_init complete");
  Serial.flush();
}

void loop() {
  cyd::app_tick();
  delay(5);
}

#endif  // UNIT_TEST
