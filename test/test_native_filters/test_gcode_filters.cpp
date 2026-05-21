// test_gcode_filters.cpp
// Unit tests for the G-code filter pipeline: filterLine, injectMissingS,
// applyPenOverwrite. Pure logic — no hardware, no serial, no SD.
//
// Run with: pio test -e test_native

#include <unity.h>
#include "gcode_filters.h"

// Required globals
int  g_penDownS   = 100;
int  g_penUpS     = 800;
bool g_overwriteS = false;

void setUp(void)    { g_overwriteS = false; g_penUpS = 800; g_penDownS = 100; }
void tearDown(void) {}

// ============================================================
// filterLine
// ============================================================

void test_filter_empty_line(void) {
    char line[] = "";
    TEST_ASSERT_FALSE(filterLine(line));
}

void test_filter_comment_semicolon(void) {
    char line[] = "; this is a comment";
    TEST_ASSERT_FALSE(filterLine(line));
}

void test_filter_T_command(void) {
    char line[] = "T0 M6";
    TEST_ASSERT_FALSE(filterLine(line));
}

void test_filter_keeps_G21(void) {
    char line[] = "G21";
    TEST_ASSERT_TRUE(filterLine(line));
    TEST_ASSERT_EQUAL_STRING("G21", line);
}

void test_filter_keeps_G90(void) {
    char line[] = "G90";
    TEST_ASSERT_TRUE(filterLine(line));
}

void test_filter_Z_only_move(void) {
    char line[] = "G0 Z5.000";
    TEST_ASSERT_FALSE(filterLine(line));
}

void test_filter_Z_only_negative(void) {
    char line[] = "G0 Z-2.5";
    TEST_ASSERT_FALSE(filterLine(line));
}

void test_filter_strips_Z_from_XY_move(void) {
    char line[] = "G0 X10 Y10 Z3.0";
    TEST_ASSERT_TRUE(filterLine(line));
    TEST_ASSERT_EQUAL_STRING("G0 X10 Y10", line);
}

void test_filter_strips_Z_from_G1_move(void) {
    char line[] = "G1 X20 Y30 Z-1.5 F1000";
    TEST_ASSERT_TRUE(filterLine(line));
    // Z token removed; rest preserved
    TEST_ASSERT_NULL(strstr(line, "Z"));
    TEST_ASSERT_NOT_NULL(strstr(line, "X20"));
    TEST_ASSERT_NOT_NULL(strstr(line, "Y30"));
}

void test_filter_strips_inline_comment(void) {
    char line[] = "G0 X10 Y10 (pen up move)";
    TEST_ASSERT_TRUE(filterLine(line));
    TEST_ASSERT_EQUAL_STRING("G0 X10 Y10", line);
}

void test_filter_keeps_M5(void) {
    char line[] = "M5";
    TEST_ASSERT_TRUE(filterLine(line));
}

void test_filter_trims_trailing_spaces(void) {
    char line[] = "G21   ";
    TEST_ASSERT_TRUE(filterLine(line));
    TEST_ASSERT_EQUAL_STRING("G21", line);
}

// ============================================================
// injectMissingS
// ============================================================

void test_inject_M5_no_S(void) {
    char line[32] = "M5";
    injectMissingS(line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("M5 S100", line);
}

void test_inject_M4_no_S(void) {
    char line[32] = "M4";
    injectMissingS(line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("M4 S800", line);
}

void test_inject_M5_already_has_S(void) {
    char line[32] = "M5 S200";
    injectMissingS(line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("M5 S200", line);  // unchanged
}

void test_inject_skips_motion_line(void) {
    // Line has X/Y — must not inject S
    char line[32] = "G1 X10 Y10";
    injectMissingS(line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("G1 X10 Y10", line);
}

void test_inject_uses_configured_values(void) {
    g_penUpS   = 900;
    g_penDownS = 50;
    char line[32] = "M4";
    injectMissingS(line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("M4 S900", line);
}

// ============================================================
// applyPenOverwrite
// ============================================================

void test_overwrite_disabled_leaves_M5(void) {
    g_overwriteS = false;
    char line[32] = "M5 S500";
    applyPenOverwrite(line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("M5 S500", line);
}

void test_overwrite_M3_becomes_pendown(void) {
    g_overwriteS = true;
    char line[32] = "M3 S1000";
    applyPenOverwrite(line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("M5 S100", line);  // M3 → pen down
}

void test_overwrite_M5_becomes_penup(void) {
    g_overwriteS = true;
    char line[32] = "M5 S500";
    applyPenOverwrite(line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("M4 S800", line);  // M5 → pen up
}

void test_overwrite_skips_motion_line(void) {
    g_overwriteS = true;
    char line[32] = "G1 X10 Y10";
    applyPenOverwrite(line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("G1 X10 Y10", line);
}

// ============================================================
// progressPathFor
// ============================================================

void test_progress_path_with_directory(void) {
    char out[64];
    progressPathFor("/files/drawing.nc", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("/drawing.nc.progress", out);
}

void test_progress_path_no_directory(void) {
    char out[64];
    progressPathFor("drawing.nc", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("/drawing.nc.progress", out);
}

// ============================================================
// Runner
// ============================================================

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // filterLine
    RUN_TEST(test_filter_empty_line);
    RUN_TEST(test_filter_comment_semicolon);
    RUN_TEST(test_filter_T_command);
    RUN_TEST(test_filter_keeps_G21);
    RUN_TEST(test_filter_keeps_G90);
    RUN_TEST(test_filter_Z_only_move);
    RUN_TEST(test_filter_Z_only_negative);
    RUN_TEST(test_filter_strips_Z_from_XY_move);
    RUN_TEST(test_filter_strips_Z_from_G1_move);
    RUN_TEST(test_filter_strips_inline_comment);
    RUN_TEST(test_filter_keeps_M5);
    RUN_TEST(test_filter_trims_trailing_spaces);

    // injectMissingS
    RUN_TEST(test_inject_M5_no_S);
    RUN_TEST(test_inject_M4_no_S);
    RUN_TEST(test_inject_M5_already_has_S);
    RUN_TEST(test_inject_skips_motion_line);
    RUN_TEST(test_inject_uses_configured_values);

    // applyPenOverwrite
    RUN_TEST(test_overwrite_disabled_leaves_M5);
    RUN_TEST(test_overwrite_M3_becomes_pendown);
    RUN_TEST(test_overwrite_M5_becomes_penup);
    RUN_TEST(test_overwrite_skips_motion_line);

    // progressPathFor
    RUN_TEST(test_progress_path_with_directory);
    RUN_TEST(test_progress_path_no_directory);

    return UNITY_END();
}
