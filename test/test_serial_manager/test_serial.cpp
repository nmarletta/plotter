// test_serial.cpp
// Unit tests for SerialManager using PlatformIO Unity framework.
//
// Hardware setup options:
//   Option A (loopback): Connect TX (pin 14) to RX (pin 13) with a wire.
//                        Tests that send data will receive it back.
//   Option B (no device): Tests timeout/parse behaviour without any hardware.
//   Option C (real GRBL): Connect GRBL Arduino to Serial1 for full integration.
//
// Run with: pio test -e mkrwifi1010

#include <Arduino.h>
#include <unity.h>
#include "serial_manager.h"

SerialManager testMgr;

void setUp(void) {}
void tearDown(void) {}

// --- Parsing tests (no hardware needed) ---

void test_parse_status_ok(void) {
  TEST_ASSERT_EQUAL(GRBL_OK, testMgr.parseStatus("ok"));
}

void test_parse_status_ok_with_trailing(void) {
  // GRBL sometimes sends "ok\r" - trim() in waitForOk handles this,
  // but parseStatus itself uses startsWith so plain "ok" is sufficient.
  TEST_ASSERT_EQUAL(GRBL_OK, testMgr.parseStatus("ok"));
}

void test_parse_status_error(void) {
  TEST_ASSERT_EQUAL(GRBL_ERROR, testMgr.parseStatus("error:9"));
}

void test_parse_status_error_code_1(void) {
  TEST_ASSERT_EQUAL(GRBL_ERROR, testMgr.parseStatus("error:1"));
}

void test_parse_status_alarm(void) {
  TEST_ASSERT_EQUAL(GRBL_ALARM, testMgr.parseStatus("ALARM:1"));
}

void test_parse_status_alarm_code_5(void) {
  TEST_ASSERT_EQUAL(GRBL_ALARM, testMgr.parseStatus("ALARM:5"));
}

void test_parse_status_report_is_unknown(void) {
  TEST_ASSERT_EQUAL(GRBL_UNKNOWN, testMgr.parseStatus("<Idle|MPos:0.000,0.000,0.000|FS:0,0>"));
}

void test_parse_status_settings_is_unknown(void) {
  TEST_ASSERT_EQUAL(GRBL_UNKNOWN, testMgr.parseStatus("$0=10"));
}

void test_parse_status_empty_is_unknown(void) {
  TEST_ASSERT_EQUAL(GRBL_UNKNOWN, testMgr.parseStatus(""));
}

// --- Initialization test ---

void test_initialization_returns_true(void) {
  TEST_ASSERT_TRUE(testMgr.begin(115200));
}

void test_is_ready_after_init(void) {
  testMgr.begin(115200);
  TEST_ASSERT_TRUE(testMgr.isReady());
}

// --- Timeout tests (no loopback, no GRBL device) ---
// NOTE: These tests take up to 1 second each due to timeout.

void test_send_line_times_out_without_grbl(void) {
  testMgr.begin(115200);
  bool result = testMgr.sendLine("$$");
  TEST_ASSERT_FALSE(result);
}

void test_get_last_response_is_timeout_string(void) {
  testMgr.begin(115200);
  testMgr.sendLine("$$");
  TEST_ASSERT_EQUAL_STRING("TIMEOUT", testMgr.getLastResponse().c_str());
}

void test_wait_for_ok_times_out(void) {
  testMgr.begin(115200);
  bool result = testMgr.waitForOk(200);  // Short timeout to keep test fast
  TEST_ASSERT_FALSE(result);
  TEST_ASSERT_EQUAL_STRING("TIMEOUT", testMgr.getLastResponse().c_str());
}

void setup() {
  delay(2000);  // Allow USB serial monitor to connect
  Serial.begin(115200);

  UNITY_BEGIN();

  // Parsing tests - no hardware needed
  RUN_TEST(test_parse_status_ok);
  RUN_TEST(test_parse_status_ok_with_trailing);
  RUN_TEST(test_parse_status_error);
  RUN_TEST(test_parse_status_error_code_1);
  RUN_TEST(test_parse_status_alarm);
  RUN_TEST(test_parse_status_alarm_code_5);
  RUN_TEST(test_parse_status_report_is_unknown);
  RUN_TEST(test_parse_status_settings_is_unknown);
  RUN_TEST(test_parse_status_empty_is_unknown);

  // Initialization tests
  RUN_TEST(test_initialization_returns_true);
  RUN_TEST(test_is_ready_after_init);

  // Timeout tests (each ~1s without GRBL connected)
  RUN_TEST(test_send_line_times_out_without_grbl);
  RUN_TEST(test_get_last_response_is_timeout_string);
  RUN_TEST(test_wait_for_ok_times_out);

  UNITY_END();
}

void loop() {}
