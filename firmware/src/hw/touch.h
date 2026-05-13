#pragma once

#include <cstdint>

namespace cyd {

enum class TouchKind { None, Capacitive, Resistive };

struct TouchEvent {
  bool pressed = false;
  int16_t x = -1;
  int16_t y = -1;
};

class Touch {
 public:
  // probe() does an I²C scan for FT6336; on success initialises capacitive
  // mode. On failure, initialises XPT2046 over VSPI in resistive mode. The
  // result is cached.
  TouchKind probe_and_init();

  TouchKind kind() const { return kind_; }

  // Non-blocking read; returns pressed=false when no touch is active.
  TouchEvent poll();

 private:
  TouchKind kind_ = TouchKind::None;
};

Touch &touch();

} // namespace cyd
