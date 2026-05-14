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

// Arduino-only: NON-BLOCKING. On the first call, prints READY <mac> on serial;
// thereafter drains any pending bytes from the serial RX buffer into an
// internal line accumulator. When a complete '\n'-terminated line arrives,
// passes it to parse_provisioning_json. Returns true only on a successful
// parse - caller is then expected to persist credentials and reboot. On parse
// failure prints "ERR <reason>" and returns false. Returns false if no
// complete line is ready yet - the common case, letting the main loop keep
// ticking LVGL and polling touch.
bool run_usb_provisioning(ProvisioningCreds &out);

} // namespace cyd
