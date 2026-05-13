#include "net/wifi_onboarding.h"

#ifndef UNIT_TEST

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>

namespace cyd {

namespace {
std::string make_ap_name() {
  uint64_t mac = ESP.getEfuseMac();
  char buf[24];
  snprintf(buf, sizeof(buf), "ClaudeMonitor-%04X",
           (unsigned)(mac & 0xFFFF));
  return buf;
}
}

std::string WifiOnboarding::ap_ssid() const {
  static std::string n = make_ap_name();
  return n;
}

bool WifiOnboarding::try_saved(const std::string &ssid, const std::string &psk) {
  if (ssid.empty()) return false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), psk.c_str());
  uint32_t deadline = millis() + 12000;
  while (millis() < deadline) {
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(200);
  }
  return false;
}

bool WifiOnboarding::run_portal(std::string &out_ssid, std::string &out_psk) {
  WiFiManager wm;
  wm.setTitle("Claude Monitor");
  wm.setConfigPortalTimeout(0);                 // wait forever
  wm.setBreakAfterConfig(true);                 // exit once creds saved
  if (!wm.startConfigPortal(ap_ssid().c_str())) {
    return false;
  }
  out_ssid = WiFi.SSID().c_str();
  out_psk  = WiFi.psk().c_str();
  return WiFi.status() == WL_CONNECTED;
}

} // namespace cyd

#endif  // UNIT_TEST
