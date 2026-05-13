#include "hw/display.h"

#ifndef UNIT_TEST

#include "hw/pins.h"

namespace cyd {

CydDisplay::CydDisplay() {
  {
    auto cfg = bus_.config();
    cfg.spi_host = HSPI_HOST;
    cfg.spi_mode = 0;
    cfg.freq_write = 40000000;
    cfg.freq_read = 16000000;
    cfg.spi_3wire = false;
    cfg.use_lock = true;
    cfg.dma_channel = SPI_DMA_CH_AUTO;
    cfg.pin_sclk = cyd::pins::TFT_SCLK;
    cfg.pin_mosi = cyd::pins::TFT_MOSI;
    cfg.pin_miso = cyd::pins::TFT_MISO;
    cfg.pin_dc   = cyd::pins::TFT_DC;
    bus_.config(cfg);
    panel_.setBus(&bus_);
  }
  {
    auto cfg = panel_.config();
    cfg.pin_cs   = cyd::pins::TFT_CS;
    cfg.pin_rst  = cyd::pins::TFT_RST;
    cfg.pin_busy = -1;
    cfg.panel_width  = 240;
    cfg.panel_height = 320;
    cfg.offset_rotation = 0;
    cfg.readable = false;
    cfg.invert = false;
    cfg.rgb_order = false;
    cfg.dlen_16bit = false;
    cfg.bus_shared = false;
    panel_.config(cfg);
  }
  {
    auto cfg = light_.config();
    cfg.pin_bl = cyd::pins::TFT_BL;
    cfg.invert = false;
    cfg.freq = 12000;
    cfg.pwm_channel = 7;
    light_.config(cfg);
    panel_.setLight(&light_);
  }
  {
    // Touch shares HSPI with the display; LovyanGFX handles CS arbitration.
    auto cfg = touch_.config();
    cfg.x_min = 300;
    cfg.x_max = 3900;
    cfg.y_min = 200;
    cfg.y_max = 3700;
    cfg.pin_int = cyd::pins::XPT_IRQ;
    cfg.bus_shared = true;
    cfg.offset_rotation = 0;
    cfg.spi_host = HSPI_HOST;
    cfg.freq = 1000000;
    cfg.pin_sclk = cyd::pins::TFT_SCLK;
    cfg.pin_mosi = cyd::pins::TFT_MOSI;
    cfg.pin_miso = cyd::pins::TFT_MISO;
    cfg.pin_cs   = cyd::pins::XPT_CS;
    touch_.config(cfg);
    panel_.setTouch(&touch_);
  }
  setPanel(&panel_);
}

CydDisplay &display() {
  static CydDisplay d;
  return d;
}

} // namespace cyd

#endif  // UNIT_TEST
