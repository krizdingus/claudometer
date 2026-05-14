#include "net/mdns_discover.h"

#ifndef UNIT_TEST

#include <ESPmDNS.h>

#include "app/app_config.h"

namespace cyd {

bool MdnsDiscover::find(DaemonAddr &out) {
  if (!MDNS.begin("claudometer-client")) return false;
  int n = MDNS.queryService("claudeusage", "tcp");
  if (n <= 0) return false;

  String host = MDNS.hostname(0);
  uint16_t port = MDNS.port(0);
  // ESPmDNS sometimes returns a result count > 0 with empty fields when the
  // service is not actually on the LAN. Treat anything without a real host
  // and port as a miss.
  if (host.length() == 0 || port == 0) return false;

  out.hostname = std::string(host.c_str()) + ".local";
  out.port = port;
  String friendly = MDNS.txt(0, "host");
  out.display = friendly.length() > 0 ? friendly.c_str() : out.hostname;
  return true;
}

} // namespace cyd

#endif  // UNIT_TEST
