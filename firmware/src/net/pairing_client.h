#pragma once

#include <string>

namespace cyd {

class PairingClient {
 public:
  // POST /v1/pair-init {cyd_id:"…"} → returns 4-digit code on success.
  bool init(const std::string &base_url, const std::string &cyd_id,
            std::string &out_code);

  // POST /v1/pair-verify {cyd_id, code, name} → returns 64-char hex token.
  bool verify(const std::string &base_url, const std::string &cyd_id,
              const std::string &code, const std::string &name,
              std::string &out_token);
};

// "CYD-XXYYZZ" derived from the bottom 3 bytes of the MAC.
std::string device_cyd_id();
std::string device_name();

} // namespace cyd
