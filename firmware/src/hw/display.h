#pragma once

#ifndef UNIT_TEST

#include <LovyanGFX.hpp>

namespace cyd {

class CydDisplay : public lgfx::LGFX_Device {
 public:
  CydDisplay();

 private:
  lgfx::Panel_ST7789 panel_;
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM light_;
  lgfx::Touch_XPT2046 touch_;
};

// Singleton accessor used everywhere.
CydDisplay &display();

} // namespace cyd

#endif  // UNIT_TEST
