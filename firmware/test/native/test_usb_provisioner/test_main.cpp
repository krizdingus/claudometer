#include <unity.h>
#include <string>

#include "net/usb_provisioner.h"

using namespace cyd;

void test_parse_valid_json_populates_all_fields(void) {
  ProvisioningCreds out;
  std::string err;
  bool ok = parse_provisioning_json(
      R"({"wifi_ssid":"Home","wifi_password":"hunter2","server_host":"192.168.1.42","server_port":7842,"bearer_token":"abc123","provision_schema":1})",
      out, err);
  TEST_ASSERT_TRUE_MESSAGE(ok, err.c_str());
  TEST_ASSERT_EQUAL_STRING("Home", out.wifi_ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("hunter2", out.wifi_password.c_str());
  TEST_ASSERT_EQUAL_STRING("192.168.1.42", out.server_host.c_str());
  TEST_ASSERT_EQUAL(7842, out.server_port);
  TEST_ASSERT_EQUAL_STRING("abc123", out.bearer_token.c_str());
}

void test_parse_rejects_missing_field(void) {
  ProvisioningCreds out;
  std::string err;
  // missing bearer_token
  bool ok = parse_provisioning_json(
      R"({"wifi_ssid":"H","wifi_password":"p","server_host":"h","server_port":7842,"provision_schema":1})",
      out, err);
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_TRUE(err.find("bearer_token") != std::string::npos);
}

void test_parse_rejects_wrong_schema(void) {
  ProvisioningCreds out;
  std::string err;
  bool ok = parse_provisioning_json(
      R"({"wifi_ssid":"H","wifi_password":"p","server_host":"h","server_port":7842,"bearer_token":"t","provision_schema":2})",
      out, err);
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_TRUE(err.find("schema") != std::string::npos);
}

void test_parse_rejects_empty_string_field(void) {
  ProvisioningCreds out;
  std::string err;
  bool ok = parse_provisioning_json(
      R"({"wifi_ssid":"","wifi_password":"p","server_host":"h","server_port":7842,"bearer_token":"t","provision_schema":1})",
      out, err);
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_TRUE(err.find("wifi_ssid") != std::string::npos);
}

void test_parse_rejects_port_out_of_range(void) {
  ProvisioningCreds out;
  std::string err;
  bool ok = parse_provisioning_json(
      R"({"wifi_ssid":"H","wifi_password":"p","server_host":"h","server_port":0,"bearer_token":"t","provision_schema":1})",
      out, err);
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_TRUE(err.find("server_port") != std::string::npos);
}

void test_parse_rejects_malformed_json(void) {
  ProvisioningCreds out;
  std::string err;
  bool ok = parse_provisioning_json("not json at all", out, err);
  TEST_ASSERT_FALSE(ok);
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_parse_valid_json_populates_all_fields);
  RUN_TEST(test_parse_rejects_missing_field);
  RUN_TEST(test_parse_rejects_wrong_schema);
  RUN_TEST(test_parse_rejects_empty_string_field);
  RUN_TEST(test_parse_rejects_port_out_of_range);
  RUN_TEST(test_parse_rejects_malformed_json);
  return UNITY_END();
}
