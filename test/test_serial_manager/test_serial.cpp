// test_serial.cpp
// Comprehensive SerialManager tests.
//
// Unit tests (parseStatus, isReady, readLine, flushRx) always run.
// GRBL-conditional tests auto-skip when no GRBL device is detected on Serial1.
//
// Hardware setup for GRBL tests:
//   MKR pin 14 (TX) -> GRBL Arduino RX
//   MKR pin 13 (RX) -> GRBL Arduino TX
//   Shared GND
//
// Run with: pio test -e test_serial_mgr

#include <Arduino.h>
#include <unity.h>
#include "serial_manager.h"

SerialManager serialMgr(Serial1);
static bool grbl_present = false;

void setUp(void) {
    serialMgr.flushRx();
}

void tearDown(void) {}

// ---- Parsing tests (always run, no hardware) ----

void test_parse_ok(void) {
    TEST_ASSERT_EQUAL(GRBL_OK, serialMgr.parseStatus("ok"));
}

void test_parse_ok_prefix(void) {
    // waitForOk strips CR/LF; parseStatus sees plain "ok"
    TEST_ASSERT_EQUAL(GRBL_OK, serialMgr.parseStatus("ok"));
}

void test_parse_error_code_1(void) {
    TEST_ASSERT_EQUAL(GRBL_ERROR, serialMgr.parseStatus("error:1"));
}

void test_parse_error_code_9(void) {
    TEST_ASSERT_EQUAL(GRBL_ERROR, serialMgr.parseStatus("error:9"));
}

void test_parse_alarm_code_1(void) {
    TEST_ASSERT_EQUAL(GRBL_ALARM, serialMgr.parseStatus("ALARM:1"));
}

void test_parse_alarm_code_5(void) {
    TEST_ASSERT_EQUAL(GRBL_ALARM, serialMgr.parseStatus("ALARM:5"));
}

void test_parse_status_report_is_unknown(void) {
    TEST_ASSERT_EQUAL(GRBL_UNKNOWN, serialMgr.parseStatus("<Idle|MPos:0.000,0.000,0.000|FS:0,0>"));
}

void test_parse_settings_line_is_unknown(void) {
    TEST_ASSERT_EQUAL(GRBL_UNKNOWN, serialMgr.parseStatus("$0=10"));
}

void test_parse_empty_is_unknown(void) {
    TEST_ASSERT_EQUAL(GRBL_UNKNOWN, serialMgr.parseStatus(""));
}

// ---- Initialisation (always runs) ----

void test_is_ready_after_begin(void) {
    TEST_ASSERT_TRUE(serialMgr.isReady());
}

void test_initial_response_not_null(void) {
    TEST_ASSERT_NOT_NULL(serialMgr.getLastResponse());
}

// ---- readLine / flushRx with empty RX buffer ----

void test_readline_returns_false_when_no_data(void) {
    serialMgr.flushRx();
    char buf[32];
    TEST_ASSERT_FALSE(serialMgr.readLine(buf, sizeof(buf)));
}

void test_flush_rx_safe_on_empty_buffer(void) {
    // Should not crash or hang when buffer is already empty
    serialMgr.flushRx();
    TEST_PASS();
}

// ---- Timeout tests (run without GRBL; each ~200 ms) ----

void test_sendline_times_out_without_grbl(void) {
    if (grbl_present) TEST_IGNORE_MESSAGE("GRBL present — timeout test skipped");
    bool ok = serialMgr.sendLine("$$", 200);
    TEST_ASSERT_FALSE(ok);
}

void test_response_is_timeout_string(void) {
    if (grbl_present) TEST_IGNORE_MESSAGE("GRBL present — timeout test skipped");
    serialMgr.sendLine("$$", 200);
    TEST_ASSERT_EQUAL_STRING("TIMEOUT", serialMgr.getLastResponse());
}

// ---- GRBL integration tests (auto-skip when GRBL absent) ----

void test_grbl_unlock_returns_ok(void) {
    if (!grbl_present) TEST_IGNORE_MESSAGE("GRBL not detected");
    TEST_ASSERT_TRUE(serialMgr.sendLine("$X", 1000));
}

void test_grbl_response_stored_after_sendline(void) {
    if (!grbl_present) TEST_IGNORE_MESSAGE("GRBL not detected");
    serialMgr.sendLine("$X", 1000);
    TEST_ASSERT_EQUAL_STRING("ok", serialMgr.getLastResponse());
}

void test_grbl_send_pen_up(void) {
    if (!grbl_present) TEST_IGNORE_MESSAGE("GRBL not detected");
    TEST_ASSERT_TRUE(serialMgr.sendLine("M4 S800", 2000));
}

void test_grbl_send_pen_down(void) {
    if (!grbl_present) TEST_IGNORE_MESSAGE("GRBL not detected");
    TEST_ASSERT_TRUE(serialMgr.sendLine("M5 S100", 2000));
}

void test_grbl_readline_gets_data(void) {
    if (!grbl_present) TEST_IGNORE_MESSAGE("GRBL not detected");
    serialMgr.flushRx();
    // Send a command bypassing sendLine so we can test readLine independently
    Serial1.println("$X");
    char buf[64];
    bool got = false;
    unsigned long start = millis();
    while (millis() - start < 1500) {
        if (serialMgr.readLine(buf, sizeof(buf))) { got = true; break; }
    }
    TEST_ASSERT_TRUE_MESSAGE(got, "readLine() should return a line after sending a command");
}

void test_grbl_query_settings_fires_callback(void) {
    if (!grbl_present) TEST_IGNORE_MESSAGE("GRBL not detected");
    static int count;
    count = 0;
    bool ok = serialMgr.querySettings([](const String& cmd, float value) {
        (void)cmd; (void)value;
        count++;
    }, 3000);
    TEST_ASSERT_TRUE_MESSAGE(ok, "querySettings should return true on success");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, count, "At least one setting should be fired");
}

void test_grbl_soft_reset_then_unlock(void) {
    if (!grbl_present) TEST_IGNORE_MESSAGE("GRBL not detected");
    serialMgr.softResetAndWait();
    // After reset GRBL enters alarm; $X should clear it and reply ok
    bool ok = serialMgr.sendLine("$X", 2000);
    TEST_ASSERT_TRUE_MESSAGE(ok, "sendLine($X) should succeed after soft reset");
}

void test_grbl_probe_status_does_not_hang(void) {
    if (!grbl_present) TEST_IGNORE_MESSAGE("GRBL not detected");
    serialMgr.flushRx();
    serialMgr.probeStatus();
    TEST_PASS();
}

// ---- Runner ----

void setup() {
    delay(2000);
    Serial.begin(115200);

    // Initialise once before all tests
    serialMgr.begin(115200);

    // Detect GRBL by sending an empty command and waiting for any response
    grbl_present = false;
    Serial1.println("");
    char probe[32];
    unsigned long start = millis();
    while (millis() - start < 500) {
        if (serialMgr.readLine(probe, sizeof(probe))) {
            grbl_present = true;
            break;
        }
    }

    UNITY_BEGIN();

    // Parsing (always run)
    RUN_TEST(test_parse_ok);
    RUN_TEST(test_parse_ok_prefix);
    RUN_TEST(test_parse_error_code_1);
    RUN_TEST(test_parse_error_code_9);
    RUN_TEST(test_parse_alarm_code_1);
    RUN_TEST(test_parse_alarm_code_5);
    RUN_TEST(test_parse_status_report_is_unknown);
    RUN_TEST(test_parse_settings_line_is_unknown);
    RUN_TEST(test_parse_empty_is_unknown);

    // Initialisation (always run)
    RUN_TEST(test_is_ready_after_begin);
    RUN_TEST(test_initial_response_not_null);

    // readLine / flushRx (always run)
    RUN_TEST(test_readline_returns_false_when_no_data);
    RUN_TEST(test_flush_rx_safe_on_empty_buffer);

    // Timeout tests (skip when GRBL is present)
    RUN_TEST(test_sendline_times_out_without_grbl);
    RUN_TEST(test_response_is_timeout_string);

    // GRBL integration (auto-skip when absent)
    RUN_TEST(test_grbl_unlock_returns_ok);
    RUN_TEST(test_grbl_response_stored_after_sendline);
    RUN_TEST(test_grbl_send_pen_down);
    RUN_TEST(test_grbl_send_pen_up);
    RUN_TEST(test_grbl_send_pen_down);
    RUN_TEST(test_grbl_readline_gets_data);
    RUN_TEST(test_grbl_query_settings_fires_callback);
    RUN_TEST(test_grbl_soft_reset_then_unlock);
    RUN_TEST(test_grbl_probe_status_does_not_hang);

    UNITY_END();
}

void loop() {}
