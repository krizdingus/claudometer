#pragma once

#include <cstdint>

namespace cyd {

#ifndef UNIT_TEST

class Nvs;
class Tileview;

class Carousel {
 public:
  void begin(Nvs *nvs, Tileview *tv);
  void tick(uint32_t now_ms, bool touch_pressed);
  void set_enabled(bool on);
  bool is_enabled() const { return enabled_; }

 private:
  Nvs *nvs_ = nullptr;
  Tileview *tv_ = nullptr;
  bool enabled_ = false;
  uint32_t last_advance_ms_ = 0;
  uint32_t last_touch_ms_ = 0;
};

#endif  // UNIT_TEST

}  // namespace cyd
