// test_serial_native.cpp
// SerialManager unit tests running on the host PC.
//
// Uses StreamMock to control exactly what bytes GRBL "sends" and to inspect
// exactly what bytes SerialManager writes to the wire — no easy pass.
//
// Run with: pio test -e test_native

#include <unity.h>
#include "stream_mock.h"
#include "serial_manager.h"

static StreamMock mock;
static SerialManager mgr(mock);

void setUp(void)    { mock.clear(); }
void tearDown(void) {}

// ============================================================
// parseStatus — pure logic, no wire
// ============================================================

void test_parse_ok(void) {
    TEST_ASSERT_EQUAL(GRBL_OK,      mgr.parseStatus("ok"));
}
void test_parse_error(void) {
    TEST_ASSERT_EQUAL(GRBL_ERROR,   mgr.parseStatus("error:9"));
}
void test_parse_error_1(void) {
    TEST_ASSERT_EQUAL(GRBL_ERROR,   mgr.parseStatus("error:1"));
}
void test_parse_alarm(void) {
    TEST_ASSERT_EQUAL(GRBL_ALARM,   mgr.parseStatus("ALARM:1"));
}
void test_parse_alarm_5(void) {
    TEST_ASSERT_EQUAL(GRBL_ALARM,   mgr.parseStatus("ALARM:5"));
}
void test_parse_status_report(void) {
    TEST_ASSERT_EQUAL(GRBL_UNKNOWN, mgr.parseStatus("<Idle|MPos:0,0,0|FS:0,0>"));
}
void test_parse_settings_line(void) {
    TEST_ASSERT_EQUAL(GRBL_UNKNOWN, mgr.parseStatus("$0=10"));
}
void test_parse_empty(void) {
    TEST_ASSERT_EQUAL(GRBL_UNKNOWN, mgr.parseStatus(""));
}

// ============================================================
// waitForOk — verify response parsing via controlled mock bytes
// ============================================================

void test_waitForOk_ok_response(void) {
    mock.pushLine("ok");
    bool result = mgr.waitForOk(100);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("ok", mgr.getLastResponse());
}

void test_waitForOk_error_response(void) {
    mock.pushLine("error:9");
    bool result = mgr.waitForOk(100);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_STRING("error:9", mgr.getLastResponse());
}

void test_waitForOk_alarm_response(void) {
    mock.pushLine("ALARM:1");
    bool result = mgr.waitForOk(100);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_STRING("ALARM:1", mgr.getLastResponse());
}

void test_waitForOk_skips_status_report(void) {
    // GRBL sends a status report, then ok — must skip the report
    mock.pushLine("<Idle|MPos:0.000,0.000,0.000|FS:0,0>");
    mock.pushLine("ok");
    bool result = mgr.waitForOk(200);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("ok", mgr.getLastResponse());
}

void test_waitForOk_skips_empty_lines(void) {
    mock.pushBytes("\r\n\r\n");
    mock.pushLine("ok");
    bool result = mgr.waitForOk(200);
    TEST_ASSERT_TRUE(result);
}

void test_waitForOk_timeout_no_response(void) {
    // Nothing in mock — must time out
    bool result = mgr.waitForOk(50);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_STRING("TIMEOUT", mgr.getLastResponse());
}

void test_waitForOk_strips_cr(void) {
    // GRBL sends CR+LF — the CR must be stripped before parsing
    mock.pushBytes("ok\r\n");
    bool result = mgr.waitForOk(100);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("ok", mgr.getLastResponse());
}

// ============================================================
// sendLine — verify exact bytes sent AND response handling
// ============================================================

void test_sendLine_writes_correct_bytes(void) {
    mock.pushLine("ok");
    mgr.sendLine("M4 S800", 100);
    // println() appends \r\n
    std::string sent = mock.txString();
    TEST_ASSERT_EQUAL_STRING("M4 S800\r\n", sent.c_str());
}

void test_sendLine_returns_true_on_ok(void) {
    mock.pushLine("ok");
    TEST_ASSERT_TRUE(mgr.sendLine("$X", 100));
}

void test_sendLine_returns_false_on_error(void) {
    mock.pushLine("error:9");
    TEST_ASSERT_FALSE(mgr.sendLine("G99", 100));
    TEST_ASSERT_EQUAL_STRING("error:9", mgr.getLastResponse());
}

void test_sendLine_returns_false_on_timeout(void) {
    // No mock response
    TEST_ASSERT_FALSE(mgr.sendLine("$$", 50));
    TEST_ASSERT_EQUAL_STRING("TIMEOUT", mgr.getLastResponse());
}

// ============================================================
// readLine — verify byte-level line assembly
// ============================================================

void test_readLine_basic(void) {
    mock.pushLine("hello");
    char buf[32];
    TEST_ASSERT_TRUE(mgr.readLine(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("hello", buf);
}

void test_readLine_strips_cr(void) {
    mock.pushBytes("hello\r\n");
    char buf[32];
    TEST_ASSERT_TRUE(mgr.readLine(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("hello", buf);
}

void test_readLine_two_lines(void) {
    mock.pushLine("line1");
    mock.pushLine("line2");
    char buf[32];
    TEST_ASSERT_TRUE(mgr.readLine(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("line1", buf);
    TEST_ASSERT_TRUE(mgr.readLine(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("line2", buf);
    TEST_ASSERT_FALSE(mgr.readLine(buf, sizeof(buf)));
}

void test_readLine_empty_stream(void) {
    char buf[32];
    TEST_ASSERT_FALSE(mgr.readLine(buf, sizeof(buf)));
}

void test_readLine_skips_bare_newline(void) {
    // A bare \n produces an empty line — must be skipped
    mock.pushBytes("\nok\n");
    char buf[32];
    TEST_ASSERT_TRUE(mgr.readLine(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("ok", buf);
}

void test_readLine_truncates_long_line(void) {
    mock.pushBytes("ABCDEFGHIJ\n");
    char buf[5];  // maxLen = 5, so max content = 4 chars
    bool got = mgr.readLine(buf, sizeof(buf));
    TEST_ASSERT_TRUE(got);
    TEST_ASSERT_EQUAL(4, (int)strlen(buf));  // truncated to maxLen-1
}

// ============================================================
// flushRx
// ============================================================

void test_flushRx_drains_stream(void) {
    mock.pushBytes("garbage\nmore\n");
    mgr.flushRx();
    char buf[32];
    TEST_ASSERT_FALSE(mgr.readLine(buf, sizeof(buf)));
}

void test_flushRx_safe_on_empty(void) {
    mgr.flushRx();  // must not crash
    TEST_PASS();
}

// ============================================================
// getLastResponse
// ============================================================

void test_getLastResponse_initially_empty(void) {
    // Constructed fresh — last response should be empty string
    StreamMock fresh;
    SerialManager m(fresh);
    const char* r = m.getLastResponse();
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_STRING("", r);
}

// ============================================================
// isReady
// ============================================================

void test_isReady_returns_true_with_mock(void) {
    TEST_ASSERT_TRUE(mgr.isReady());
}

// ============================================================
// Runner
// ============================================================

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // parseStatus
    RUN_TEST(test_parse_ok);
    RUN_TEST(test_parse_error);
    RUN_TEST(test_parse_error_1);
    RUN_TEST(test_parse_alarm);
    RUN_TEST(test_parse_alarm_5);
    RUN_TEST(test_parse_status_report);
    RUN_TEST(test_parse_settings_line);
    RUN_TEST(test_parse_empty);

    // waitForOk
    RUN_TEST(test_waitForOk_ok_response);
    RUN_TEST(test_waitForOk_error_response);
    RUN_TEST(test_waitForOk_alarm_response);
    RUN_TEST(test_waitForOk_skips_status_report);
    RUN_TEST(test_waitForOk_skips_empty_lines);
    RUN_TEST(test_waitForOk_timeout_no_response);
    RUN_TEST(test_waitForOk_strips_cr);

    // sendLine
    RUN_TEST(test_sendLine_writes_correct_bytes);
    RUN_TEST(test_sendLine_returns_true_on_ok);
    RUN_TEST(test_sendLine_returns_false_on_error);
    RUN_TEST(test_sendLine_returns_false_on_timeout);

    // readLine
    RUN_TEST(test_readLine_basic);
    RUN_TEST(test_readLine_strips_cr);
    RUN_TEST(test_readLine_two_lines);
    RUN_TEST(test_readLine_empty_stream);
    RUN_TEST(test_readLine_skips_bare_newline);
    RUN_TEST(test_readLine_truncates_long_line);

    // flushRx
    RUN_TEST(test_flushRx_drains_stream);
    RUN_TEST(test_flushRx_safe_on_empty);

    // getLastResponse / isReady
    RUN_TEST(test_getLastResponse_initially_empty);
    RUN_TEST(test_isReady_returns_true_with_mock);

    return UNITY_END();
}
