#include <unity.h>
#include <fstream>
#include <sstream>
#include <string>

#include "net/stats_parser.h"
#include "net/stats_types.h"

static std::string read_fixture() {
  std::ifstream f("test/fixtures/stats_full.json");
  TEST_ASSERT_TRUE_MESSAGE(f.good(), "fixture not found");
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

void test_parse_full_payload(void) {
  std::string body = read_fixture();
  cyd::Stats out;
  bool ok = cyd::parse_stats(body.c_str(), body.size(), out);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_INT(1, out.schema);
  TEST_ASSERT_EQUAL_INT(67, out.session.pct_used);
  TEST_ASSERT_EQUAL_INT(98, out.session.minutes_remaining);
  TEST_ASSERT_EQUAL_STRING("16:01", out.session.resets_at.c_str());
  TEST_ASSERT_EQUAL_size_t(2, out.session.models.size());
  TEST_ASSERT_EQUAL_INT(412000, out.session.models[0].tokens);

  TEST_ASSERT_EQUAL_INT(612000, out.models_today.total_tokens);
  TEST_ASSERT_EQUAL_size_t(3, out.models_today.by_model.size());
  TEST_ASSERT_FLOAT_WITHIN(0.001, 7.41, out.models_today.est_cost_usd);

  TEST_ASSERT_EQUAL_INT(58, out.sonnet.weekly_pct);
  TEST_ASSERT_EQUAL_STRING("on_track", out.sonnet.pace.c_str());

  TEST_ASSERT_EQUAL_INT(42, out.chat.messages_today);
  TEST_ASSERT_EQUAL_INT(200, out.chat.daily_cap);

  TEST_ASSERT_EQUAL_size_t(2, out.routines.size());
  TEST_ASSERT_EQUAL_STRING("babysit-prs", out.routines[0].name.c_str());
  TEST_ASSERT_EQUAL_STRING("slow", out.routines[1].status.c_str());

  TEST_ASSERT_EQUAL_INT(71, out.budgets.code_all);
  TEST_ASSERT_EQUAL_INT(89, out.budgets.code_opus);
  TEST_ASSERT_EQUAL_STRING("max-20x", out.budgets.plan.c_str());
  TEST_ASSERT_EQUAL_STRING("3d19h", out.budgets.resets_in.c_str());
}

void test_parse_rejects_garbage(void) {
  cyd::Stats out;
  const char *junk = "not json at all";
  TEST_ASSERT_FALSE(cyd::parse_stats(junk, 16, out));
}

void test_parse_handles_missing_optional_fields(void) {
  // Minimal payload: only schema + empty containers. Parser must accept it
  // (the daemon may someday omit fields with no data).
  std::string body = R"({"schema":1,"generated_at":"2026-05-13T00:00:00Z",
    "session":{"pct_used":0,"minutes_remaining":0,"resets_at":"","models":[]},
    "models_today":{"total_tokens":0,"by_model":[],"est_cost_usd":0},
    "sonnet":{"weekly_pct":0,"used":0,"cap":0,"pace":""},
    "chat":{"messages_today":0,"daily_cap":0,"resets_at":""},
    "routines":[],
    "budgets":{"code_all":0,"code_opus":0,"chat":0,"plan":"","resets_in":""}})";
  cyd::Stats out;
  TEST_ASSERT_TRUE(cyd::parse_stats(body.c_str(), body.size(), out));
  TEST_ASSERT_EQUAL_INT(1, out.schema);
  TEST_ASSERT_TRUE(out.routines.empty());
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_parse_full_payload);
  RUN_TEST(test_parse_rejects_garbage);
  RUN_TEST(test_parse_handles_missing_optional_fields);
  return UNITY_END();
}
