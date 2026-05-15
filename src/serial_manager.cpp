// serial_manager.cpp
// Implementation of SerialManager for GRBL serial communication.
// Uses Serial1 hardware UART (TX=pin14, RX=pin13) exclusively for GRBL.
// CRITICAL: Never use Serial1 for debug output - Serial only for USB debug.

#include "serial_manager.h"

bool SerialManager::begin(unsigned long baud) {
  Serial1.begin(baud);
  // Flush any GRBL startup greeting messages
  unsigned long startTime = millis();
  while (millis() - startTime < 500) {
    if (Serial1.available()) {
      Serial1.read();
    }
  }
  _lastResponse = "";
  return true;
}

bool SerialManager::sendLine(const char* line) {
  Serial1.println(line);
  return waitForOk(3000); // 3s: GRBL writes all settings to EEPROM (~250-500ms) before sending ok
}

bool SerialManager::waitForOk(unsigned int timeout_ms) {
  unsigned long deadline = millis() + timeout_ms;
  char buf[64];
  uint8_t len = 0;
  int totalBytes = 0;
  while (millis() < deadline) {
    if (!Serial1.available()) continue;
    char c = (char)Serial1.read();
    totalBytes++;
    if (c == '\r') continue;
    if (c == '\n') {
      buf[len] = '\0';
      if (len > 0) {
        Serial.print("GRBL:["); Serial.print(buf); Serial.println(']');
        String s(buf);
        GRBLStatus st = parseStatus(s);
        if (st == GRBL_OK)    { _lastResponse = s; return true; }
        if (st == GRBL_ERROR || st == GRBL_ALARM) { _lastResponse = s; return false; }
        // status report or unknown line — keep waiting
      }
      len = 0;
    } else if (len < sizeof(buf) - 1) {
      buf[len++] = c;
    }
  }
  Serial.print("GRBL:TIMEOUT("); Serial.print(totalBytes); Serial.println("bytes)");
  _lastResponse = "TIMEOUT";
  return false;
}

String SerialManager::getLastResponse() {
  return _lastResponse;
}

GRBLStatus SerialManager::parseStatus(const String& response) {
  if (response.startsWith("ok")) {
    return GRBL_OK;
  } else if (response.startsWith("error:")) {
    return GRBL_ERROR;
  } else if (response.startsWith("ALARM:")) {
    return GRBL_ALARM;
  }
  return GRBL_UNKNOWN;
}

bool SerialManager::isReady() {
  // Serial1 is a hardware UART - it is ready after begin() is called
  return Serial1;
}

void SerialManager::softResetAndWait() {
  Serial.println("GRBL: soft reset...");
  Serial1.write(0x18); // CTRL-X: GRBL real-time soft reset
  // GRBL startup banner is ~60 bytes; 500ms is ample for the AVR to reboot
  // and enter STATE_ALARM (which suppresses the hard-limit ISR).
  unsigned long deadline = millis() + 500;
  int n = 0;
  while (millis() < deadline) {
    if (Serial1.available()) { Serial1.read(); n++; }
  }
  Serial.print("GRBL: reset done, discarded "); Serial.print(n); Serial.println(" bytes");
}

bool SerialManager::querySettings(void (*onSetting)(const String& cmd, float value), unsigned int timeout_ms) {
  // flush any pending input
  while (Serial1.available()) Serial1.read();

  Serial1.println("$$");
  Serial.println("QSET: sent $$");

  unsigned long deadline = millis() + timeout_ms;
  char buf[64];
  uint8_t len = 0;
  int nSettings = 0;
  while (millis() < deadline) {
    if (!Serial1.available()) continue;
    char c = (char)Serial1.read();
    if (c == '\r') continue;
    if (c == '\n') {
      buf[len] = '\0';
      if (len > 0) {
        String line(buf);
        if (line.startsWith("ok")) {
          Serial.print("QSET: ok, "); Serial.print(nSettings); Serial.println(" settings loaded");
          return true;
        }
        if (line.startsWith("error") || line.startsWith("ALARM")) {
          Serial.print("QSET: rejected: "); Serial.println(line);
          return false;
        }
        if (line.startsWith("$")) {
          int eq = line.indexOf('=');
          if (eq > 0) {
            String cmd = line.substring(0, eq);
            float val  = line.substring(eq + 1).toFloat();
            onSetting(cmd, val);
            nSettings++;
          }
        }
      }
      len = 0;
    } else if (len < sizeof(buf) - 1) {
      buf[len++] = c;
    }
  }
  Serial.print("QSET: TIMEOUT, got "); Serial.print(nSettings); Serial.println(" settings before timeout");
  return false; // timeout
}
