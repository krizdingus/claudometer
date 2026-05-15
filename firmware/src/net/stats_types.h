#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cyd {

struct ModelTokenRow {
  std::string model;
  int tokens = 0;
};

struct SessionStat {
  int pct_used = 0;
  int minutes_remaining = 0;
  std::string resets_at;
  std::vector<ModelTokenRow> models;
};

struct ModelsTodayStat {
  int total_tokens = 0;
  std::vector<ModelTokenRow> by_model;
  double est_cost_usd = 0.0;
};

struct ChatStat {
  int messages_today = 0;
  int daily_cap = 0;
  std::string resets_at;
};

struct RoutineStat {
  std::string name;
  std::string status;
  std::string last_run;
  std::string next_run;
  int next_run_in_minutes = -1;  // -1 if no upcoming run
};

struct BudgetsStat {
  int code_all = 0;
  int code_opus = 0;
  int chat = 0;
  std::string plan;
  std::string resets_in;
};

struct Stats {
  int schema = 0;
  std::string generated_at;
  std::string local_time; // HH:MM from daemon host's local TZ
  SessionStat session;
  ModelsTodayStat models_today;
  ChatStat chat;
  std::vector<RoutineStat> routines;
  BudgetsStat budgets;
  bool stale = false; // set true when daemon last-known data is being shown
};

} // namespace cyd
