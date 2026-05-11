// test_serial_grbl.cpp
// GRBL-connected integration tests for SerialManager.
//
// These tests require a GRBL Arduino connected to Serial1 (TX=pin14, RX=pin13).
// If no GRBL device responds within 500ms of a wake signal, all GRBL-dependent
// tests are automatically skipped — the suite still passes cleanly.
//
// Hardware setup:
//   - Arduino MKR WiFi 1010 pin 14 (TX) → GRBL Arduino RX
//   - Arduino MKR WiFi 1010 pin 13 (RX) → GRBL Arduino TX
//   - Shared GND between the two boards
//
// Run with: pio test -e mkrwifi1010

#include <Arduino.h>
#include <unity.h>
#include "serial_manager.h"

// Global SerialManager instance (mirrors globals.cpp)
SerialManager serialMgr;

// Set to true in setUp() if a GRBL device is detected
static bool grbl_present = false;

void setUp(void) {
  serialMgr.begin(115200);

  // Flush any stale bytes
  while (Serial1.available()) Serial1.read();

  // Send a wake signal and check for any response within 500ms
  Serial1.println("");
  unsigned long start = millis();
  while (millis() - start < 500UL) {
    if (Serial1.available()) {
      String line = Serial1.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) {
        grbl_present = true;
        break;
      }
    }
  }
}

void tearDown(void) {}

// ---- Always-runs test ----

void test_is_ready_after_begin(void) {
  TEST_ASSERT_TRUE(serialMgr.isReady());
}

// ---- GRBL-conditional tests ----

void test_grbl_wake_response(void) {
  if (!grbl_present) {
    TEST_IGNORE_MESSAGE("GRBL not detected — skipping wake response test");
  }
  // Flush and re-wake to get a fresh response
  while (Serial1.available()) Serial1.read();
  Serial1.println("");

  bool got_response = false;
  unsigned long start = millis();
  while (millis() - start < 2000UL) {
    if (Serial1.available()) {
      String line = Serial1.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) {
        got_response = true;
        break;
      }
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(got_response, "Expected non-empty response from GRBL after wake");
}

void test_grbl_settings_query(void) {
  if (!grbl_present) {
    TEST_IGNORE_MESSAGE("GRBL not detected — skipping settings query test");
  }
  // Flush and send $$ settings request
  while (Serial1.available()) Serial1.read();
  Serial1.println("$$");

  bool found_setting_line = false;
  unsigned long start = millis();
  while (millis() - start < 3000UL) {
    if (Serial1.available()) {
      String line = Serial1.readStringUntil('\n');
      line.trim();
      // GRBL settings lines look like "$0=10 (step pulse, usec)"
      if (line.length() > 0 && line.indexOf('$') >= 0) {
        found_setting_line = true;
        break;
      }
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(found_setting_line, "Expected at least one '$' settings line in $$ response");
}

void setup() {
  delay(2000); // Allow USB serial monitor to connect
  Serial.begin(115200);

  UNITY_BEGIN();

  // Always runs
  RUN_TEST(test_is_ready_after_begin);

  // GRBL-conditional (self-skip if device absent)
  RUN_TEST(test_grbl_wake_response);
  RUN_TEST(test_grbl_settings_query);

  UNITY_END();
}

void loop() {}
