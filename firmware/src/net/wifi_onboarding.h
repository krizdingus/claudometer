#pragma once

#include <string>

namespace cyd {

class WifiOnboarding {
 public:
  // Returns the AP SSID we'll advertise, e.g. "ClaudeMonitor-7A23".
  std::string ap_ssid() const;

  // Try saved credentials with a 12s timeout. Returns true if connected.
  bool try_saved(const std::string &ssid, const std::string &psk);

  // Block until the user finishes the captive portal flow. Returns the SSID
  // and PSK chosen so the caller can save them to NVS. The portal page is
  // titled "Claude Monitor".
  bool run_portal(std::string &out_ssid, std::string &out_psk);
};

} // namespace cyd
