#pragma once

#include <cstdint>

// CYD ESP32-2432S028R/C standard pin assignments.
// The resistive (R) and capacitive (C) variants share the same display pins;
// only the touch interface differs.

namespace cyd::pins {

// ILI9341 TFT (HSPI)
constexpr int TFT_SCLK = 14;
constexpr int TFT_MOSI = 13;
constexpr int TFT_MISO = 12;
constexpr int TFT_CS   = 15;
constexpr int TFT_DC   = 2;
constexpr int TFT_RST  = -1;   // tied to EN on most boards
constexpr int TFT_BL   = 21;   // backlight (active high)

// XPT2046 resistive touch (VSPI) — R variant. The CYD wires the touch
// controller to its own SPI bus, separate from the display's HSPI.
constexpr int XPT_SCLK = 25;
constexpr int XPT_MOSI = 32;
constexpr int XPT_MISO = 39;
constexpr int XPT_CS   = 33;
constexpr int XPT_IRQ  = 36;

// FT6336 capacitive touch (I²C) — C variant only.
// NOTE: these pins overlap with XPT2046 on R boards; runtime detection picks one.
constexpr int FT_SDA = 33;
constexpr int FT_SCL = 32;
constexpr uint8_t FT_ADDR = 0x38;

} // namespace cyd::pins
