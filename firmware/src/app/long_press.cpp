#include "app/long_press.h"

namespace cyd {

bool LongPress::update(bool pressed, uint32_t now_ms, uint32_t hold_ms) {
  if (!pressed) {
    pressed_since_ = 0;
    fired_ = false;
    return false;
  }
  if (pressed_since_ == 0) {
    pressed_since_ = now_ms;
    return false;
  }
  if (!fired_ && now_ms - pressed_since_ >= hold_ms) {
    fired_ = true;
    return true;
  }
  return false;
}

} // namespace cyd
