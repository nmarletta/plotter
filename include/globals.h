#ifndef globals_h
#define globals_h

#include <Arduino.h>
#include "serial_manager.h"

enum State {
  MAIN,
  CONTROL,
  SETTINGS,
  PEN_SETTINGS,
  FILES,
  PLOT,
  HWTEST,
  WIFI
};

struct Setting {
  String command;
  String name;
  String unit;
  float value;
  float minValue;
  float maxValue;
  int decimals;
  float step;
};

extern State currentState;
extern SerialManager serialMgr;
extern String selectedFile; // Set by state_files before entering PLOT state

// Pen configuration — set in Pen Settings, persisted to /.config.cfg on SD
extern int  g_penDownS;    // S value sent for pen down
extern int  g_penUpS;      // S value sent for pen up
extern bool g_overwriteS;  // if true, M3/M5 S values in gcode are replaced
extern int  g_penDelayMs;  // G4 dwell (ms) injected after each pen command; 0 = disabled

#endif
