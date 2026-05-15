#include "net/stats_parser.h"

#include <ArduinoJson.h>

namespace cyd {

static ModelTokenRow read_row(JsonObjectConst obj) {
  ModelTokenRow r;
  r.model = obj["model"].as<const char *>() ? obj["model"].as<const char *>() : "";
  r.tokens = obj["tokens"] | 0;
  return r;
}

bool parse_stats(const char *data, size_t len, Stats &out) {
  // 8 KB doc per the spec. Payload is typically ~2 KB but routines may grow.
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) return false;

  out = Stats{};
  out.schema = doc["schema"] | 0;
  if (out.schema != 1) return false;
  out.generated_at = doc["generated_at"] | "";
  out.local_time = doc["local_time"] | "";

  JsonObjectConst s = doc["session"].as<JsonObjectConst>();
  out.session.pct_used = s["pct_used"] | 0;
  out.session.minutes_remaining = s["minutes_remaining"] | 0;
  out.session.resets_at = s["resets_at"] | "";
  for (JsonObjectConst row : s["models"].as<JsonArrayConst>()) {
    out.session.models.push_back(read_row(row));
  }

  JsonObjectConst mt = doc["models_today"].as<JsonObjectConst>();
  out.models_today.total_tokens = mt["total_tokens"] | 0;
  out.models_today.est_cost_usd = mt["est_cost_usd"] | 0.0;
  for (JsonObjectConst row : mt["by_model"].as<JsonArrayConst>()) {
    out.models_today.by_model.push_back(read_row(row));
  }

  JsonObjectConst ch = doc["chat"].as<JsonObjectConst>();
  out.chat.messages_today = ch["messages_today"] | 0;
  out.chat.daily_cap = ch["daily_cap"] | 0;
  out.chat.resets_at = ch["resets_at"] | "";

  for (JsonObjectConst r : doc["routines"].as<JsonArrayConst>()) {
    RoutineStat rs;
    rs.name = r["name"] | "";
    rs.status = r["status"] | "";
    rs.last_run = r["last_run"] | "";
    rs.next_run = r["next_run"] | "";
    rs.next_run_in_minutes = r["next_run_in_minutes"].is<int>() ? r["next_run_in_minutes"].as<int>() : -1;
    out.routines.push_back(rs);
  }

  JsonObjectConst b = doc["budgets"].as<JsonObjectConst>();
  out.budgets.code_all = b["code_all"] | 0;
  out.budgets.code_opus = b["code_opus"] | 0;
  out.budgets.chat = b["chat"] | 0;
  out.budgets.plan = b["plan"] | "";
  out.budgets.resets_in = b["resets_in"] | "";

  return true;
}

} // namespace cyd
