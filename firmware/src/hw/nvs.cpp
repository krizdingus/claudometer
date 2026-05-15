#include "hw/nvs.h"

#ifndef UNIT_TEST

#include <Preferences.h>

#include "app/app_config.h"

namespace cyd {

namespace {
constexpr const char *kNs = "cydmon";
}

static Preferences prefs;

void Nvs::begin() {
  prefs.begin(kNs, /*readOnly=*/false);
}

bool Nvs::has_wifi_creds() const { return prefs.isKey("wifi_ssid"); }
std::string Nvs::wifi_ssid() const { return prefs.getString("wifi_ssid", "").c_str(); }
std::string Nvs::wifi_psk()  const { return prefs.getString("wifi_psk",  "").c_str(); }
void Nvs::save_wifi(const std::string &ssid, const std::string &psk) {
  prefs.putString("wifi_ssid", ssid.c_str());
  prefs.putString("wifi_psk",  psk.c_str());
}

bool Nvs::has_server() const { return prefs.isKey("srv_host") && prefs.isKey("srv_port"); }
std::string Nvs::server_host() const { return prefs.getString("srv_host", "").c_str(); }
uint16_t Nvs::server_port() const { return (uint16_t)prefs.getUInt("srv_port", 0); }
void Nvs::save_server(const std::string &host, uint16_t port) {
  prefs.putString("srv_host", host.c_str());
  prefs.putUInt("srv_port", port);
}

bool Nvs::has_bearer_token() const { return prefs.isKey("bearer"); }
std::string Nvs::bearer_token() const { return prefs.getString("bearer", "").c_str(); }
void Nvs::save_bearer_token(const std::string &token) {
  prefs.putString("bearer", token.c_str());
}

bool Nvs::has_touch_cal() const { return prefs.isKey("touch_cal"); }
void Nvs::load_touch_cal(uint16_t cal[8]) const {
  prefs.getBytes("touch_cal", cal, sizeof(uint16_t) * 8);
}
void Nvs::save_touch_cal(const uint16_t cal[8]) {
  prefs.putBytes("touch_cal", cal, sizeof(uint16_t) * 8);
}

bool Nvs::has_theme() const { return prefs.isKey("theme"); }
int Nvs::theme_mode() const { return prefs.getInt("theme", 0); }
void Nvs::save_theme(int mode) { prefs.putInt("theme", mode); }

bool Nvs::has_brightness() const { return prefs.isKey("brightness"); }
uint8_t Nvs::brightness() const { return (uint8_t)prefs.getUChar("brightness", kBrightnessDefault); }
void Nvs::save_brightness(uint8_t duty) { prefs.putUChar("brightness", duty); }

bool Nvs::has_carousel() const { return prefs.isKey("carousel"); }
bool Nvs::carousel() const { return prefs.getBool("carousel", false); }
void Nvs::save_carousel(bool on) { prefs.putBool("carousel", on); }

void Nvs::factory_reset() {
  prefs.clear();
}

} // namespace cyd

#endif // UNIT_TEST
