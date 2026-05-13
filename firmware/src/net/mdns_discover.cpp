#include "net/mdns_discover.h"

#ifndef UNIT_TEST

#include <ESPmDNS.h>

#include "app/app_config.h"

namespace cyd {

bool MdnsDiscover::find(DaemonAddr &out) {
  if (!MDNS.begin("cydmonitor-client")) return false;
  int n = MDNS.queryService("claudeusage", "tcp");
  if (n <= 0) return false;

  out.hostname = std::string(MDNS.hostname(0).c_str()) + ".local";
  out.port = MDNS.port(0);
  // TXT record: host=<friendly>
  String friendly = MDNS.txt(0, "host");
  out.display = friendly.length() > 0 ? friendly.c_str() : out.hostname;
  return true;
}

} // namespace cyd

#endif  // UNIT_TEST
