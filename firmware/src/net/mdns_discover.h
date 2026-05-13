#pragma once

#include <string>
#include <cstdint>

namespace cyd {

struct DaemonAddr {
  std::string hostname;   // e.g., "krizzos-mbp.local"
  std::string display;    // friendly name from TXT host=...
  uint16_t port = 0;
};

class MdnsDiscover {
 public:
  // One-shot blocking query for _claudeusage._tcp. Returns false if no
  // service found within the timeout (see kMdnsQueryMs in app_config.h).
  bool find(DaemonAddr &out);
};

} // namespace cyd
