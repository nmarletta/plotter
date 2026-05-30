#ifndef grbl_errors_h
#define grbl_errors_h

#include <Arduino.h>

// Returns a short human-readable string for a GRBL error code.
// Pass the numeric part after "error:" — e.g. grblErrorStr(15) → "Soft limit hit"
inline const char* grblErrorStr(int code) {
  switch (code) {
    case  1: return "Expected cmd letter";
    case  2: return "Bad number format";
    case  3: return "Invalid $ statement";
    case  4: return "Negative value";
    case  5: return "Homing disabled";
    case  7: return "EEPROM read fail";
    case  8: return "Not idle";
    case  9: return "Locked / alarm";
    case 10: return "Soft limits need home";
    case 11: return "Line too long";
    case 12: return "Step rate exceeded";
    case 15: return "Soft limit hit";
    case 16: return "Jog cmd invalid";
    case 17: return "Jog step error";
    case 20: return "Unknown G-code";
    case 22: return "No feed rate set";
    case 25: return "Word repeated";
    case 27: return "No axis words";
    default: {
      static char buf[14];
      snprintf(buf, sizeof(buf), "error:%d", code);
      return buf;
    }
  }
}

// Parses a raw GRBL response line like "error:15" or "ALARM:1"
// and returns a short display string.
inline const char* grblResponseStr(const char* line) {
  if (strncmp(line, "error:", 6) == 0) return grblErrorStr(atoi(line + 6));
  if (strncmp(line, "ALARM:", 6) == 0) {
    switch (atoi(line + 6)) {
      case 1: return "Hard limit";
      case 2: return "Soft limit";
      case 3: return "Reset/motion";
      case 7: return "Homing: no sw";
      case 8: return "Homing: failed";
      case 9: return "Homing: pulloff";
      default: { static char b[14]; snprintf(b, sizeof(b), "ALARM:%d", atoi(line+6)); return b; }
    }
  }
  if (strcmp(line, "ok") == 0)      return "ok";
  if (strcmp(line, "TIMEOUT") == 0) return "Timeout";
  return line;
}

#endif
