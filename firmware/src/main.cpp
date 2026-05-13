// firmware/src/main.cpp
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("cydmonitor firmware booting");
}

void loop() {
  delay(1000);
}
