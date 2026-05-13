#pragma once

#include <cstdint>

namespace cyd {

class LongPress {
 public:
  // Call every tick with current pressed state. Returns true exactly once
  // when the touch has been continuously pressed for hold_ms.
  bool update(bool pressed, uint32_t now_ms, uint32_t hold_ms);

 private:
  uint32_t pressed_since_ = 0;
  bool fired_ = false;
};

} // namespace cyd
