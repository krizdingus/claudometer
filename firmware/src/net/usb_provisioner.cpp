#include "net/usb_provisioner.h"

#include <ArduinoJson.h>

namespace cyd {

namespace {

bool require_nonempty_string(JsonDocument &doc, const char *key,
                             std::string &out, std::string &err) {
  if (!doc[key].is<const char *>()) {
    err = std::string("missing or non-string: ") + key;
    return false;
  }
  out = doc[key].as<const char *>();
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

  if (!doc["provision_schema"].is<int>() || doc["provision_schema"].as<int>() != 1) {
    err = "unsupported provision_schema";
    return false;
  }

  if (!require_nonempty_string(doc, "wifi_ssid",     out.wifi_ssid,     err)) return false;
  if (!require_nonempty_string(doc, "wifi_password", out.wifi_password, err)) return false;
  if (!require_nonempty_string(doc, "server_host",   out.server_host,   err)) return false;
  if (!require_nonempty_string(doc, "bearer_token",  out.bearer_token,  err)) return false;

  if (!doc["server_port"].is<int>()) {
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

// Arduino-side body lives here but is defined in the next task.

#endif // UNIT_TEST

} // namespace cyd
