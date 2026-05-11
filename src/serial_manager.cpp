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
  return waitForOk(1000);
}

bool SerialManager::waitForOk(unsigned int timeout_ms) {
  unsigned long startTime = millis();
  while (millis() - startTime < timeout_ms) {
    if (Serial1.available()) {
      String response = Serial1.readStringUntil('\n');
      response.trim();  // Strip \r and whitespace
      GRBLStatus status = parseStatus(response);
      if (status == GRBL_OK) {
        _lastResponse = response;
        return true;
      } else if (status == GRBL_ERROR || status == GRBL_ALARM) {
        _lastResponse = response;
        return false;
      }
      // Ignore status reports and unknown lines, keep waiting
    }
  }
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
