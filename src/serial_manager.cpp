// serial_manager.cpp
#include "serial_manager.h"
#include "logger.h"

// ---- Constructors ----------------------------------------------------------

#ifdef ARDUINO
SerialManager::SerialManager(HardwareSerial& grbl)
    : _grbl(grbl), _hw(&grbl) { _lastResponse[0] = '\0'; }
#endif

SerialManager::SerialManager(Stream& grbl)
    : _grbl(grbl)
#ifdef ARDUINO
    , _hw(nullptr)
#endif
    { _lastResponse[0] = '\0'; }

// ---- Public methods --------------------------------------------------------

bool SerialManager::begin(unsigned long baud) {
#ifdef ARDUINO
  if (_hw) _hw->begin(baud);
#endif
  unsigned long startTime = millis();
  while (millis() - startTime < 500) {
    if (_grbl.available()) _grbl.read();
  }
  _lastResponse[0] = '\0';
  _rxLen = 0;
  return true;
}

bool SerialManager::sendLine(const char* line, unsigned int timeout_ms) {
  Log::send(line);
  _grbl.println(line);
  return waitForOk(timeout_ms);
}

bool SerialManager::waitForOk(unsigned int timeout_ms) {
  unsigned long deadline = millis() + timeout_ms;
  char buf[64];
  uint8_t len = 0;
  int totalBytes = 0;
  while (millis() < deadline) {
    if (!_grbl.available()) continue;
    char c = (char)_grbl.read();
    totalBytes++;
    if (c == '\r') continue;
    if (c == '\n') {
      buf[len] = '\0';
      if (len > 0) {
        Log::recv(buf);
        GRBLStatus st = parseStatus(buf);
        if (st == GRBL_OK) {
          strncpy(_lastResponse, buf, sizeof(_lastResponse) - 1);
          _lastResponse[sizeof(_lastResponse)-1] = '\0';
          return true;
        }
        if (st == GRBL_ERROR || st == GRBL_ALARM) {
          strncpy(_lastResponse, buf, sizeof(_lastResponse) - 1);
          _lastResponse[sizeof(_lastResponse)-1] = '\0';
          return false;
        }
        // status report or unknown — keep waiting
      }
      len = 0;
    } else if (len < sizeof(buf) - 1) {
      buf[len++] = c;
    }
  }
  { char t[32]; snprintf(t, sizeof(t), "TIMEOUT (%d bytes)", totalBytes); Log::err(t); }
  strncpy(_lastResponse, "TIMEOUT", sizeof(_lastResponse) - 1);
  return false;
}

const char* SerialManager::getLastResponse() {
  return _lastResponse;
}

GRBLStatus SerialManager::parseStatus(const char* response) {
  if (strncmp(response, "ok",     2) == 0) return GRBL_OK;
  if (strncmp(response, "error:", 6) == 0) return GRBL_ERROR;
  if (strncmp(response, "ALARM:", 6) == 0) return GRBL_ALARM;
  return GRBL_UNKNOWN;
}

bool SerialManager::isReady() {
#ifdef ARDUINO
  if (_hw) return (bool)*_hw;
#endif
  return true;
}

bool SerialManager::readLine(char* buf, uint8_t maxLen) {
  while (_grbl.available()) {
    char c = (char)_grbl.read();
    if (c == '\r') continue;
    if (c == '\n') {
      _rxBuf[_rxLen] = '\0';
      if (_rxLen > 0) {
        uint8_t copyLen = (_rxLen < maxLen - 1) ? _rxLen : (uint8_t)(maxLen - 1);
        memcpy(buf, _rxBuf, copyLen);
        buf[copyLen] = '\0';
        _rxLen = 0;
        return true;
      }
      _rxLen = 0;
    } else if (_rxLen < sizeof(_rxBuf) - 1) {
      _rxBuf[_rxLen++] = c;
    }
  }
  return false;
}

void SerialManager::flushRx() {
  while (_grbl.available()) _grbl.read();
  _rxLen = 0;
}

void SerialManager::probeStatus() {
  _grbl.write('?');
  delay(150);
  if (_grbl.available()) {
    char buf[80]; uint8_t len = 0;
    while (_grbl.available() && len < sizeof(buf) - 1) buf[len++] = (char)_grbl.read();
    buf[len] = '\0';
    Log::nav(buf);
  } else {
    Log::nav("GRBL unresponsive to ?");
  }
}

void SerialManager::recoverUart() {
#ifdef ARDUINO
  if (!_hw) return;
  _hw->end();
  delay(20);
  _hw->begin(115200);
  delay(50);
  while (_grbl.available()) _grbl.read();
#endif
  Log::nav("Serial1 reinitialized");
}

void SerialManager::softResetAndWait() {
  Log::nav("soft reset");
  Log::sendByte(0x18);
  _grbl.write((uint8_t)0x18);
  unsigned long deadline = millis() + 500;
  int n = 0;
  while (millis() < deadline) {
    if (_grbl.available()) { _grbl.read(); n++; }
  }
  { char t[40]; snprintf(t, sizeof(t), "reset done, discarded %d bytes", n); Log::nav(t); }
}

bool SerialManager::querySettings(void (*onSetting)(const String& cmd, float value), unsigned int timeout_ms) {
  while (_grbl.available()) _grbl.read();
  Log::send("$$");
  _grbl.println("$$");

  unsigned long deadline = millis() + timeout_ms;
  char buf[64];
  uint8_t len = 0;
  int nSettings = 0;
  while (millis() < deadline) {
    if (!_grbl.available()) continue;
    char c = (char)_grbl.read();
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
  return false;
}
