// serial_manager.h
// SerialManager: Handles bidirectional serial communication with GRBL.
//
// Inject any Stream for testing; on Arduino pass Serial1 (HardwareSerial).

#ifndef SERIAL_MANAGER_H
#define SERIAL_MANAGER_H

#include <Arduino.h>

enum GRBLStatus {
  GRBL_OK,
  GRBL_ERROR,
  GRBL_ALARM,
  GRBL_TIMEOUT,
  GRBL_UNKNOWN
};

class SerialManager {
public:
#ifdef ARDUINO
  // Production: pass Serial1. Enables begin(baud) and recoverUart().
  explicit SerialManager(HardwareSerial& grbl);
#endif
  // Testing: inject any Stream. begin() skips baud-rate init; recoverUart() is a no-op.
  explicit SerialManager(Stream& grbl);

  bool begin(unsigned long baud = 115200);
  bool sendLine(const char* line, unsigned int timeout_ms = 3000);
  bool waitForOk(unsigned int timeout_ms = 1000);
  const char* getLastResponse();
  GRBLStatus parseStatus(const char* response);
  bool querySettings(void (*onSetting)(const String& cmd, float value), unsigned int timeout_ms = 2000);
  void softResetAndWait();
  void recoverUart();
  bool isReady();
  bool readLine(char* buf, uint8_t maxLen);
  void flushRx();
  void probeStatus();

private:
  Stream& _grbl;
#ifdef ARDUINO
  HardwareSerial* _hw;  // non-null only when constructed with HardwareSerial
#endif
  char    _lastResponse[64];
  char    _rxBuf[64];
  uint8_t _rxLen = 0;
};

#endif // SERIAL_MANAGER_H
