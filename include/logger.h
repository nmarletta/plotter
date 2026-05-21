#pragma once
#include <Arduino.h>

// Uniform tagged logger — all output goes to USB Serial monitor.
//
//  [>>]  bytes sent to GRBL over Serial1
//  [<<]  bytes received from GRBL
//  [NAV] menu / state-machine event
//  [ERR] timeout or unexpected response

namespace Log {
#ifdef ARDUINO
  inline void send(const char *cmd) {
    Serial.print("[>>]  "); Serial.println(cmd);
  }
  inline void recv(const char *resp) {
    Serial.print("[<<]  "); Serial.println(resp);
  }
  inline void nav(const char *msg) {
    Serial.print("[NAV] "); Serial.println(msg);
  }
  inline void err(const char *msg) {
    Serial.print("[ERR] "); Serial.println(msg);
  }
  inline void sendByte(uint8_t b) {
    Serial.print("[>>]  <0x"); Serial.print(b, HEX); Serial.println('>');
  }
#else
  inline void send(const char *cmd)    { printf("[>>]  %s\n", cmd); }
  inline void recv(const char *resp)   { printf("[<<]  %s\n", resp); }
  inline void nav(const char *msg)     { printf("[NAV] %s\n", msg); }
  inline void err(const char *msg)     { printf("[ERR] %s\n", msg); }
  inline void sendByte(uint8_t b)      { printf("[>>]  <0x%02X>\n", b); }
#endif
}
