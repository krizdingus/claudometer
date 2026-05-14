#include "net/usb_provisioner.h"

#include <ArduinoJson.h>

#ifndef UNIT_TEST
#include <Arduino.h>
#include <WiFi.h>
#endif

namespace cyd {

namespace {

bool require_nonempty_string(JsonDocument &doc, const char *key,
                             std::string &out, std::string &err) {
  JsonVariantConst v = doc[key];
  if (v.isNull()) {
    err = std::string("missing or non-string: ") + key;
    return false;
  }
  const char *s = v.as<const char *>();
  if (!s) {
    err = std::string("missing or non-string: ") + key;
    return false;
  }
  out = s;
  if (out.empty()) {
    err = std::string("empty: ") + key;
    return false;
  }
  return true;
}

} // namespace

bool parse_provisioning_json(const std::string &json_line,
                             ProvisioningCreds &out,
                             std::string &err) {
  JsonDocument doc;
  DeserializationError de = deserializeJson(doc, json_line);
  if (de) {
    err = std::string("json: ") + de.c_str();
    return false;
  }

  if (doc["provision_schema"].as<int>() != 1) {
    err = "unsupported provision_schema";
    return false;
  }

  if (!require_nonempty_string(doc, "wifi_ssid",     out.wifi_ssid,     err)) return false;
  if (!require_nonempty_string(doc, "wifi_password", out.wifi_password, err)) return false;
  if (!require_nonempty_string(doc, "server_host",   out.server_host,   err)) return false;
  if (!require_nonempty_string(doc, "bearer_token",  out.bearer_token,  err)) return false;

  if (doc["server_port"].isNull()) {
    err = "missing or non-int: server_port";
    return false;
  }
  int port = doc["server_port"].as<int>();
  if (port <= 0 || port > 65535) {
    err = "server_port out of range";
    return false;
  }
  out.server_port = (uint16_t)port;

  return true;
}

#ifndef UNIT_TEST

namespace {

constexpr size_t kMaxLineBytes = 1024;
std::string g_line_buffer;
bool g_ready_printed = false;

// Drain whatever bytes are currently available in the serial RX buffer into
// g_line_buffer. Never blocks. Returns true if a complete '\n'-terminated line
// was assembled; the line (without trailing '\n') is moved into out and the
// buffer is reset. Buffer overflow drops accumulated bytes and resyncs.
bool poll_serial_line(std::string &out) {
  while (Serial.available()) {
    int b = Serial.read();
    if (b < 0) continue;
    if (b == '\n') {
      out = std::move(g_line_buffer);
      g_line_buffer.clear();
      return true;
    }
    if (b == '\r') continue;
    if (g_line_buffer.size() >= kMaxLineBytes) {
      g_line_buffer.clear();
      continue;
    }
    g_line_buffer.push_back((char)b);
  }
  return false;
}

} // namespace

bool run_usb_provisioning(ProvisioningCreds &out) {
  if (!g_ready_printed) {
    uint8_t mac[6] = {0};
    WiFi.macAddress(mac);
    Serial.printf("READY %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    g_ready_printed = true;
  }

  std::string line;
  if (!poll_serial_line(line)) return false;
  if (line.empty()) return false;

  std::string err;
  if (parse_provisioning_json(line, out, err)) {
    Serial.print("OK\n");
    return true;
  }
  Serial.printf("ERR %s\n", err.c_str());
  return false;
}

#endif // UNIT_TEST

} // namespace cyd
