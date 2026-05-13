#pragma once

#include <cstdint>
#include <string>

namespace cyd {

// Thin wrapper over Arduino-ESP32 Preferences. All values live in one NVS
// namespace ("cydmon"). Keys are < 15 chars as required by Preferences.
class Nvs {
 public:
  void begin();

  // WiFi credentials.
  bool has_wifi_creds() const;
  std::string wifi_ssid() const;
  std::string wifi_psk() const;
  void save_wifi(const std::string &ssid, const std::string &psk);

  // Daemon pairing token (64-char lowercase hex).
  bool has_token() const;
  std::string token() const;
  void save_token(const std::string &token);

  // Last-known daemon host:port (e.g., "krizzo-mbp.local:7842").
  std::string daemon_host() const;
  void save_daemon_host(const std::string &host);

  // Touch calibration (resistive only). Returns true if cal values were stored.
  bool has_touch_cal() const;
  void load_touch_cal(uint16_t cal[8]) const;
  void save_touch_cal(const uint16_t cal[8]);

  // Wipe all keys, return to factory state.
  void factory_reset();
};

} // namespace cyd
