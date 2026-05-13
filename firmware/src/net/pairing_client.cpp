#include "net/pairing_client.h"

#ifndef UNIT_TEST

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "app/app_config.h"

namespace cyd {

namespace {
std::string mac_suffix() {
  uint64_t m = ESP.getEfuseMac();
  char b[8];
  snprintf(b, sizeof(b), "%06X", (unsigned)(m & 0xFFFFFF));
  return b;
}
}

std::string device_cyd_id() {
  return std::string("CYD-") + mac_suffix();
}

std::string device_name() {
  return std::string("cyd-") + mac_suffix();
}

static bool post_json(const std::string &url, const std::string &body,
                      std::string &out) {
  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  if (!http.begin(url.c_str())) return false;
  http.addHeader("Content-Type", "application/json");
  int code = http.POST((uint8_t *)body.data(), body.size());
  if (code < 200 || code >= 300) { http.end(); return false; }
  out = http.getString().c_str();
  http.end();
  return true;
}

bool PairingClient::init(const std::string &base_url,
                         const std::string &cyd_id,
                         std::string &out_code) {
  JsonDocument req;
  req["cyd_id"] = cyd_id;
  std::string body;
  serializeJson(req, body);
  std::string resp;
  if (!post_json(base_url + "/v1/pair-init", body, resp)) return false;
  JsonDocument doc;
  if (deserializeJson(doc, resp)) return false;
  const char *c = doc["code"] | "";
  if (!*c) return false;
  out_code = c;
  return true;
}

bool PairingClient::verify(const std::string &base_url,
                           const std::string &cyd_id,
                           const std::string &code,
                           const std::string &name,
                           std::string &out_token) {
  JsonDocument req;
  req["cyd_id"] = cyd_id;
  req["code"]   = code;
  req["name"]   = name;
  std::string body;
  serializeJson(req, body);
  std::string resp;
  if (!post_json(base_url + "/v1/pair-verify", body, resp)) return false;
  JsonDocument doc;
  if (deserializeJson(doc, resp)) return false;
  const char *t = doc["token"] | "";
  if (!*t) return false;
  out_token = t;
  return true;
}

} // namespace cyd

#endif  // UNIT_TEST
