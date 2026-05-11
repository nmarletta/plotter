// serial_manager.h
// SerialManager: Handles bidirectional serial communication with GRBL Arduino.
// All GRBL communication goes through Serial1 (TX=pin14, RX=pin13) at 115200 baud.

#ifndef SERIAL_MANAGER_H
#define SERIAL_MANAGER_H

#include <Arduino.h>

// GRBL response status codes
enum GRBLStatus {
  GRBL_OK,       // "ok" - command accepted
  GRBL_ERROR,    // "error:X" - command error
  GRBL_ALARM,    // "ALARM:X" - alarm condition (requires $X to clear)
  GRBL_TIMEOUT,  // No response within timeout period
  GRBL_UNKNOWN   // Unknown or status report response
};

class SerialManager {
public:
  // Initialize Serial1 at given baud rate. Returns true on success.
  bool begin(unsigned long baud);

  // Send a single G-code line to GRBL and wait for ok/error. Returns true on ok.
  bool sendLine(const char* line);

  // Block until GRBL responds ok/error or timeout_ms elapses. Returns true on ok.
  bool waitForOk(unsigned int timeout_ms = 1000);

  // Retrieve the last stored GRBL response or error string.
  String getLastResponse();

  // Parse a GRBL response string and return its status type.
  GRBLStatus parseStatus(const String& response);

  // Returns true if Serial1 is initialized and ready for communication.
  bool isReady();

private:
  String _lastResponse;  // Stores last GRBL response (error message, timeout, etc.)
};

#endif // SERIAL_MANAGER_H
