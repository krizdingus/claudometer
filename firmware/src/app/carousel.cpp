#include "app/carousel.h"

#ifndef UNIT_TEST
#include <Arduino.h>

#include "app/app_config.h"
#include "hw/nvs.h"
#include "ui/tileview.h"
#endif

namespace cyd {

#ifndef UNIT_TEST

void Carousel::begin(Nvs *nvs, Tileview *tv) {
  nvs_ = nvs;
  tv_ = tv;
  enabled_ = nvs_->carousel();
  last_advance_ms_ = millis();
  // Pretend the last touch was just over the resume window ago, so the
  // first auto-advance fires at +kCarouselIntervalMs after boot, not
  // +kCarouselResumeMs. uint32_t subtraction wraps; that's fine.
  last_touch_ms_ = millis() - kCarouselResumeMs - 1;
}

void Carousel::tick(uint32_t now_ms, bool touch_pressed) {
  if (touch_pressed) last_touch_ms_ = now_ms;
  if (!enabled_) return;
  if (now_ms - last_touch_ms_ < kCarouselResumeMs) return;
  if (!tv_) return;

  Screen current = tv_->active();
  if (current == SCR_SETTINGS || current == SCR_DEVICE) return;
  if (now_ms - last_advance_ms_ < kCarouselIntervalMs) return;

  Screen next = static_cast<Screen>((current + 1) % SCR_COUNT);
  if (next == SCR_SETTINGS) next = SCR_HOME;  // skip Settings+Device by jumping to Home
  tv_->set_active(next, LV_ANIM_ON);
  last_advance_ms_ = now_ms;
}

void Carousel::set_enabled(bool on) {
  enabled_ = on;
  if (nvs_) nvs_->save_carousel(on);
  Serial.printf("carousel: %s\n", on ? "on" : "off");
  if (on) last_advance_ms_ = millis();
}

#endif  // UNIT_TEST

}  // namespace cyd
