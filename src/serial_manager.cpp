// serial_manager.cpp
// Implementation of SerialManager for GRBL serial communication.
// Uses Serial1 hardware UART (TX=pin14, RX=pin13) exclusively for GRBL.
// CRITICAL: Never use Serial1 for debug output - Serial only for USB debug.

#include "serial_manager.h"
#include "logger.h"

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
  Log::send(line);
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
        Log::recv(buf);
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
  { char t[32]; snprintf(t, sizeof(t), "TIMEOUT (%d bytes)", totalBytes); Log::err(t); }
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

void SerialManager::recoverUart() {
  // Re-initialize the SAMD21 SERCOM to clear any framing/overrun error flags
  // that cause subsequent reads to return nothing despite GRBL sending data.
  Serial1.end();
  delay(20);
  Serial1.begin(115200);
  delay(50);
  while (Serial1.available()) Serial1.read();
  Log::nav("Serial1 reinitialized");
}

void SerialManager::softResetAndWait() {
  Log::nav("soft reset");
  Log::sendByte(0x18);
  Serial1.write(0x18); // CTRL-X: GRBL real-time soft reset
  // GRBL startup banner is ~60 bytes; 500ms is ample for the AVR to reboot
  // and enter STATE_ALARM (which suppresses the hard-limit ISR).
  unsigned long deadline = millis() + 500;
  int n = 0;
  while (millis() < deadline) {
    if (Serial1.available()) { Serial1.read(); n++; }
  }
  { char t[40]; snprintf(t, sizeof(t), "reset done, discarded %d bytes", n); Log::nav(t); }
}

bool SerialManager::querySettings(void (*onSetting)(const String& cmd, float value), unsigned int timeout_ms) {
  // flush any pending input
  while (Serial1.available()) Serial1.read();

  Log::send("$$");
  Serial1.println("$$");
  Log::nav("QSET: sent");

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
          { char t[40]; snprintf(t, sizeof(t), "QSET: ok, %d settings", nSettings); Log::nav(t); }
          return true;
        }
        if (line.startsWith("error") || line.startsWith("ALARM")) {
          { char t[72]; snprintf(t, sizeof(t), "QSET rejected: %s", line.c_str()); Log::err(t); }
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
  { char t[48]; snprintf(t, sizeof(t), "QSET: TIMEOUT, got %d settings", nSettings); Log::err(t); }
  return false; // timeout
}
