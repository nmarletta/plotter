// test_grbl.cpp
// Phase 2: PC talks to real GRBL (Arduino Uno) over USB serial.
// No MKR, no SD, no display. The PC takes the place of the MKR.
//
// Run with:
//   GRBL_PORT=/dev/cu.usbmodemXXXX pio test -e test_native_grbl
//
// If GRBL_PORT is unset, the test tries to auto-detect a USB-serial port.
// All tests that require a live GRBL connection call TEST_IGNORE if none found.

#include <unity.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glob.h>

#include "posix_stream.h"
#include "serial_manager.h"
#include "gcode_streamer.h"
#include "stdio_line_source.h"

// ---- Required globals -------------------------------------------------------

int  g_penDownS   = 100;
int  g_penUpS     = 800;
bool g_overwriteS = false;

// ---- Infrastructure ---------------------------------------------------------

static PosixStream     grblStream;
SerialManager          serialMgr(grblStream);    // satisfies extern in globals.h
static StdioLineSource fileSrc;
static GCodeStreamer    streamer(grblStream, fileSrc);

static bool grbl_connected = false;

// Temporary G-code file written to /tmp for the streaming test.
static char tmp_file[256] = "/tmp/test_phase2_grbl.nc";

// A simple Print implementation that writes to stdout, used by trace().
class StdoutPrint : public Print {
public:
    size_t write(uint8_t c) override { putchar((int)c); return 1; }
};

// ---- Port detection ---------------------------------------------------------

static bool tryPort(const char* port) {
    PosixStream probe;
    if (!probe.open(port, 115200)) return false;
    usleep(500000);  // wait for GRBL boot banner

    char buf[64];
    unsigned long start = millis();
    // Drain and look for any response within 1 s.
    while (millis() - start < 1000) {
        if (probe.available()) {
            probe.close();
            return true;
        }
        usleep(10000);
    }
    probe.close();
    return false;
}

static const char* detectPort() {
    const char* env = getenv("GRBL_PORT");
    if (env && env[0]) return env;

    // Try common macOS/Linux patterns.
    static const char* patterns[] = {
        "/dev/cu.usbmodem*",
        "/dev/ttyUSB*",
        "/dev/ttyACM*",
        nullptr
    };
    static char detected[256] = "";
    for (int i = 0; patterns[i]; i++) {
        glob_t g;
        if (glob(patterns[i], 0, nullptr, &g) == 0 && g.gl_pathc > 0) {
            strncpy(detected, g.gl_pathv[0], sizeof(detected) - 1);
            globfree(&g);
            return detected;
        }
        globfree(&g);
    }
    return nullptr;
}

// ---- setUp / tearDown -------------------------------------------------------

void setUp(void) {
    if (grbl_connected) serialMgr.flushRx();
}

void tearDown(void) {}

// ============================================================
// SerialManager — against real GRBL
// ============================================================

void test_unlock_ok(void) {
    if (!grbl_connected) TEST_IGNORE_MESSAGE("GRBL not connected");
    bool ok = serialMgr.sendLine("$X", 1000);
    // $X returns "ok" if unlocked or "[MSG:Caution: Unlocked]" + "ok".
    // Either way sendLine returns true when it sees "ok".
    TEST_ASSERT_TRUE_MESSAGE(ok, "$X did not get ok");
}

void test_version_query_ok(void) {
    if (!grbl_connected) TEST_IGNORE_MESSAGE("GRBL not connected");
    TEST_ASSERT_TRUE(serialMgr.sendLine("$I", 1000));
    // Response contains version info; sendLine returns true on "ok".
}

void test_bad_command_returns_error(void) {
    if (!grbl_connected) TEST_IGNORE_MESSAGE("GRBL not connected");
    bool ok = serialMgr.sendLine("G99", 1000);
    TEST_ASSERT_FALSE_MESSAGE(ok, "Expected error response for invalid G99");
    TEST_ASSERT_NOT_NULL(strstr(serialMgr.getLastResponse(), "error"));
}

void test_status_query_response(void) {
    if (!grbl_connected) TEST_IGNORE_MESSAGE("GRBL not connected");
    // Send real-time '?' status request; read a line; it should start with '<'.
    grblStream.write('?');
    usleep(50000);
    char buf[80];
    bool got = serialMgr.readLine(buf, sizeof(buf));
    TEST_ASSERT_TRUE_MESSAGE(got, "No response to '?' status query");
    TEST_ASSERT_EQUAL_CHAR_MESSAGE('<', buf[0], "Status report should start with '<'");
}

void test_timeout_on_empty(void) {
    if (!grbl_connected) TEST_IGNORE_MESSAGE("GRBL not connected");
    // Flush RX, then wait for ok with nothing sent — must time out.
    serialMgr.flushRx();
    bool ok = serialMgr.waitForOk(100);
    TEST_ASSERT_FALSE_MESSAGE(ok, "Expected timeout");
    TEST_ASSERT_EQUAL_STRING("TIMEOUT", serialMgr.getLastResponse());
}

// ============================================================
// GCodeStreamer — against real GRBL, file from PC filesystem
// ============================================================

void test_trace_runs_on_pc_file(void) {
    // Write a test gcode file to /tmp.
    const char* content =
        "; header comment\n"
        "G21\n"
        "G90\n"
        "G0 Z5.0\n"         // Z-only → skipped
        "G0 X10 Y10 Z3.0\n" // Z stripped
        "G1 X20 Y20 F1000\n"
        "M5\n"
        "M4\n";

    FILE* f = fopen(tmp_file, "w");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "Cannot write tmp gcode file");
    fputs(content, f);
    fclose(f);

    // Run trace() with stdout; verify it doesn't crash and opens the file.
    StdoutPrint out;
    printf("\n=== trace output ===\n");
    streamer.trace(tmp_file, out);
    printf("====================\n");

    TEST_PASS_MESSAGE("trace() completed on PC file");
}

void test_stream_dwells_to_completion(void) {
    if (!grbl_connected) TEST_IGNORE_MESSAGE("GRBL not connected");

    // Three short dwells: accepted in any GRBL state, no physical movement.
    const char* content =
        "G4 P0.05\n"
        "G4 P0.05\n"
        "G4 P0.05\n";

    FILE* f = fopen(tmp_file, "w");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "Cannot write tmp gcode file");
    fputs(content, f);
    fclose(f);

    serialMgr.sendLine("$X", 1000);
    streamer.begin();
    bool started = streamer.start(tmp_file);
    TEST_ASSERT_TRUE_MESSAGE(started, "streamer.start() returned false");

    unsigned long deadline = millis() + 15000UL;
    while (millis() < deadline) {
        streamer.tick();
        GCodeStatus s = streamer.status();
        if (s == GCodeStatus::Completed || s == GCodeStatus::Error ||
            s == GCodeStatus::Canceled  || s == GCodeStatus::Alarm) break;
        usleep(1000);
    }

    TEST_ASSERT_EQUAL_INT_MESSAGE(
        (int)GCodeStatus::Completed,
        (int)streamer.status(),
        "Expected GCodeStatus::Completed");
}

void test_stream_line_count(void) {
    if (!grbl_connected) TEST_IGNORE_MESSAGE("GRBL not connected");
    // After the previous test the streamer should have advanced through 3 lines
    // (the 3 G4 dwells), plus the auto-appended final M4. currentLine() tracks
    // "ok" count, so it should equal 4 (3 dwells + 1 final M4).
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(4, streamer.currentLine(),
        "Expected 4 lines acknowledged (3 dwells + final pen lift)");
}

void test_error_on_missing_file(void) {
    // start() with a non-existent file must return false and set Error status.
    GCodeStreamer fresh(grblStream, fileSrc);
    fresh.begin();
    bool ok = fresh.start("/tmp/does_not_exist_xyz.nc");
    TEST_ASSERT_FALSE_MESSAGE(ok, "start() should fail for missing file");
    TEST_ASSERT_EQUAL_INT((int)GCodeStatus::Error, (int)fresh.status());
}

// ============================================================
// Runner
// ============================================================

int main(int argc, char** argv) {
    const char* port = detectPort();

    if (port) {
        printf("Trying GRBL port: %s\n", port);
        if (grblStream.open(port, 115200)) {
            delay(2000);  // wait for GRBL banner / USB CDC enumeration
            serialMgr.flushRx();
            grbl_connected = true;
            printf("GRBL connected on %s\n", port);
        } else {
            printf("WARNING: Cannot open %s\n", port);
        }
    } else {
        printf("WARNING: No serial port found — set GRBL_PORT env var\n");
        printf("         Hardware tests will be IGNORED\n");
    }

    UNITY_BEGIN();

    // SerialManager vs real GRBL
    RUN_TEST(test_unlock_ok);
    RUN_TEST(test_version_query_ok);
    RUN_TEST(test_bad_command_returns_error);
    RUN_TEST(test_status_query_response);
    RUN_TEST(test_timeout_on_empty);

    // GCodeStreamer
    RUN_TEST(test_trace_runs_on_pc_file);
    RUN_TEST(test_stream_dwells_to_completion);
    RUN_TEST(test_stream_line_count);
    RUN_TEST(test_error_on_missing_file);

    grblStream.close();
    return UNITY_END();
}
