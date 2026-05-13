#include "hw/nvs.h"

#ifndef UNIT_TEST

#include <Preferences.h>

namespace cyd {

static const char *kNs = "cydmon";
static const char *kSsid = "wifi_ssid";
static const char *kPsk  = "wifi_psk";
static const char *kTok  = "tok";
static const char *kHost = "host";
static const char *kCal  = "tcal";

void Nvs::begin() {
  Preferences p;
  p.begin(kNs, false);
  p.end();
}

bool Nvs::has_wifi_creds() const {
  Preferences p; p.begin(kNs, true);
  bool ok = p.isKey(kSsid) && p.isKey(kPsk);
  p.end();
  return ok;
}

std::string Nvs::wifi_ssid() const {
  Preferences p; p.begin(kNs, true);
  String s = p.getString(kSsid, "");
  p.end();
  return std::string(s.c_str());
}

std::string Nvs::wifi_psk() const {
  Preferences p; p.begin(kNs, true);
  String s = p.getString(kPsk, "");
  p.end();
  return std::string(s.c_str());
}

void Nvs::save_wifi(const std::string &ssid, const std::string &psk) {
  Preferences p; p.begin(kNs, false);
  p.putString(kSsid, ssid.c_str());
  p.putString(kPsk, psk.c_str());
  p.end();
}

bool Nvs::has_token() const {
  Preferences p; p.begin(kNs, true);
  bool ok = p.isKey(kTok);
  p.end();
  return ok;
}

std::string Nvs::token() const {
  Preferences p; p.begin(kNs, true);
  String s = p.getString(kTok, "");
  p.end();
  return std::string(s.c_str());
}

void Nvs::save_token(const std::string &t) {
  Preferences p; p.begin(kNs, false);
  p.putString(kTok, t.c_str());
  p.end();
}

std::string Nvs::daemon_host() const {
  Preferences p; p.begin(kNs, true);
  String s = p.getString(kHost, "");
  p.end();
  return std::string(s.c_str());
}

void Nvs::save_daemon_host(const std::string &h) {
  Preferences p; p.begin(kNs, false);
  p.putString(kHost, h.c_str());
  p.end();
}

bool Nvs::has_touch_cal() const {
  Preferences p; p.begin(kNs, true);
  bool ok = p.isKey(kCal);
  p.end();
  return ok;
}

void Nvs::load_touch_cal(uint16_t cal[8]) const {
  Preferences p; p.begin(kNs, true);
  p.getBytes(kCal, cal, sizeof(uint16_t) * 8);
  p.end();
}

void Nvs::save_touch_cal(const uint16_t cal[8]) {
  Preferences p; p.begin(kNs, false);
  p.putBytes(kCal, cal, sizeof(uint16_t) * 8);
  p.end();
}

void Nvs::factory_reset() {
  Preferences p; p.begin(kNs, false);
  p.clear();
  p.end();
}

} // namespace cyd

#endif  // UNIT_TEST
