#pragma once
#include <Arduino.h>

// Uniform tagged logger — all output goes to USB Serial monitor.
//
//  [>>]  bytes sent to GRBL over Serial1
//  [<<]  bytes received from GRBL
//  [NAV] menu / state-machine event
//  [ERR] timeout or unexpected response

namespace Log {
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
  // Single-byte real-time commands (0x85 jog cancel, '?' status, 0x18 reset …)
  inline void sendByte(uint8_t b) {
    Serial.print("[>>]  <0x"); Serial.print(b, HEX); Serial.println('>');
  }
}
