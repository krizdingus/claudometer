#include "net/stats_client.h"

#ifndef UNIT_TEST

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "app/app_config.h"
#include "net/stats_parser.h"

namespace cyd {

static const char *kScreenNames[] = {
    "session", "models", "sonnet", "chat", "routines", "budgets"
};

static std::string mask_to_query(uint8_t mask) {
  std::string q;
  for (int i = 0; i < SCR_COUNT; ++i) {
    if (!(mask & (1 << i))) continue;
    if (!q.empty()) q += ",";
    q += kScreenNames[i];
  }
  return q;
}

bool StatsClient::fetch(const std::string &base_url, const std::string &token,
                        uint8_t screen_mask, Stats &out) {
  std::string url = base_url + "/v1/stats?screens=" + mask_to_query(screen_mask);
  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  if (!http.begin(url.c_str())) return false;
  http.addHeader("Authorization", std::string("Bearer " + token).c_str());
  http.addHeader("Accept-Encoding", "gzip");
  int code = http.GET();
  if (code != 200) { http.end(); return false; }
  String body = http.getString();
  http.end();
  return parse_stats(body.c_str(), body.length(), out);
}

} // namespace cyd

#endif  // UNIT_TEST
