#pragma once

#include <string>
#include <cstdint>

#include "net/stats_types.h"

namespace cyd {

class StatsClient {
 public:
  // GET /v1/stats?screens=… with Authorization: Bearer <token>. Returns true
  // on 200 + valid JSON. mask bit i → SCR enum index i.
  bool fetch(const std::string &base_url, const std::string &token,
             uint8_t screen_mask, Stats &out);
};

} // namespace cyd
