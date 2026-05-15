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

  // Daemon server address (host without scheme/port, plus port).
  bool has_server() const;
  std::string server_host() const;
  uint16_t server_port() const;
  void save_server(const std::string &host, uint16_t port);

  // Bearer token for the daemon (64-char lowercase hex, provisioned over USB).
  bool has_bearer_token() const;
  std::string bearer_token() const;
  void save_bearer_token(const std::string &token);

  // Touch calibration (resistive only). Returns true if cal values were stored.
  bool has_touch_cal() const;
  void load_touch_cal(uint16_t cal[8]) const;
  void save_touch_cal(const uint16_t cal[8]);

  // Theme mode: 0 = dark (default), 1 = light.
  bool has_theme() const;
  int theme_mode() const;       // returns 0 if unset
  void save_theme(int mode);

  // Brightness duty (0-255). Default = kBrightnessDefault when unset.
  bool has_brightness() const;
  uint8_t brightness() const;
  void save_brightness(uint8_t duty);

  // Carousel auto-rotation: true = cycle screens, false = static.
  bool has_carousel() const;
  bool carousel() const;             // returns false if unset (off by default)
  void save_carousel(bool on);

  // Wipe all keys, return to factory state.
  void factory_reset();
};

} // namespace cyd
