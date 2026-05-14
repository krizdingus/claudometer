#pragma once

#include <cstdint>
#include <string>

namespace cyd {

struct ProvisioningCreds {
  std::string wifi_ssid;
  std::string wifi_password;
  std::string server_host;
  uint16_t server_port = 0;
  std::string bearer_token;
};

// Pure function: parse one JSON line into ProvisioningCreds. Returns true on
// success. On failure, populates err with a short reason that names the
// offending field (so the host can show a useful error to the user).
bool parse_provisioning_json(const std::string &json_line,
                             ProvisioningCreds &out,
                             std::string &err);

// Arduino-only: blocks reading from Serial, prints READY <mac>, reads one line
// of JSON, parses it. On parse failure prints "ERR <reason>" and returns false
// (caller can retry). On parse success returns true and prints "OK" — caller
// is expected to write to NVS and reboot.
bool run_usb_provisioning(ProvisioningCreds &out);

} // namespace cyd
