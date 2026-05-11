#ifndef globals_h
#define globals_h

#include <Arduino.h>
#include "serial_manager.h"

enum State {
  MAIN,
  CONTROL,
  SETTINGS,
  FILES,
  PLOT,
  HWTEST
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

#endif