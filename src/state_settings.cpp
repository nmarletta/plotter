#include "state_settings.h"

Setting settings[] = {
  { " ", "<- Back", "", -1, -1, -1, -1, -1 },
  { "$0", "Step Pulse Length", "µsec", -1, 0, 999, 0, 1 },
  { "$1", "Step Idle Delay", "msec", -1, 0, 999, 0, 10 },
  { "$2", "Step Port Invert", "mask", -1, 0, 7, 0, 1 },
  { "$3", "Dir. Port Invert", "mask", -1, 0, 7, 0, 1 },
  { "$4", "Step Enable Invert", "bool", -1, 0, 1, 0, 1 },
  { "$5", "Limit Pins Invert", "bool", -1, 0, 1, 0, 1 },
  { "$11", "Junction Deviation", "mm", -1, 0, 1.0, 3, 0.01 },
  { "$12", "Arc Tolerance", "mm", -1, 0, 1.0, 3, 0.01 },
  { "$20", "Soft Limits", "bool", -1, 0, 1, 0, 1 },
  { "$21", "Hard Limits", "bool", -1, 0, 1, 0, 1 },
  { "$22", "Homing Cycle", "bool", -1, 0, 1, 0, 1 },
  { "$23", "Homing Dir Invert", "mask", -1, 0, 7, 0, 1 },
  { "$24", "Homing Feed", "mm/sec", -1, 0, 9999, 0, 10 },
  { "$25", "Homing Seek", "mm/sec", -1, 0, 9999, 0, 10 },
  { "$26", "Homing Debounce", "msec", -1, 0, 9999, 0, 10 },
  { "$27", "Homing Pull-off", "mm", -1, 0, 99, 0, 1 },
  { "$100", "X steps", "steps/mm", -1, 0, 9999, 0, 10 },
  { "$101", "Y steps", "steps/mm", -1, 0, 9999, 0, 10 },
  { "$110", "X Max Rate", "mm/min", -1, 0, 9999, 0, 100 },
  { "$111", "Y Max Rate", "mm/min", -1, 0, 9999, 0, 100 },
  { "$120", "X Acceleration", "mm/sec^2", -1, 0, 999, 0, 10 },
  { "$121", "Y Acceleration", "mm/sec^2", -1, 0, 999, 0, 10 },
  { "$130", "X Max Travel", "mm", -1, 0, 9999, 0, 10 },
  { "$131", "Y Max Travel", "mm", -1, 0, 9999, 0, 10 },
  { "#PEN_DOWN", "Pen Down", "", -1, 0, 31, 0, 1 },
  { "#PEN_UP", "Pen Up", "", -1, 0, 31, 0, 1 },
};

String settingNames[] = {
    "<- Back", "Step Pulse Length", "Step Idle Delay", "Step Port Invert", "Dir. Port Invert", "Step Enable Invert", "Limit Pins Invert", "Junction Deviation", "Arc Tolerance",
    "Soft Limits", "Hard Limits", "Homing Cycle", "Homing Dir Invert", "Homing Feed", "Homing Seek", "Homing Debounce", "Homing Pull-off", "X steps", "Y steps",
    "X Max Rate", "Y Max Rate", "X Acceleration", "Y Acceleration", "X Max Travel", "Y Max Travel", "Pen Down", "Pen Up"
};
int settingsSize = 27;


bool settings_active = false;
bool changeSetting = false;
int selectedSetting = 0;

void state_settings() {
  if (!settings_active) {
    encoder.setPosition(1);
    loadSettings();
    displayList(encoder.getPosition(), settingNames, settingsSize);
    settings_active = true;
  }

  handleEncoderChange();
  handleButtonPress();
}

void handleEncoderChange() {
  if (encoder.turned()) {
    if (changeSetting) {
      encoder.constrainPosition(settings[selectedSetting].minValue / settings[selectedSetting].step, settings[selectedSetting].maxValue / settings[selectedSetting].step);
      settings[selectedSetting].value = encoder.getPosition() * settings[selectedSetting].step;
    } else {
      encoder.constrainPosition(0, settingsSize - 1);
      selectedSetting = encoder.getPosition();
    }
    updateDisplay();
  }
}

void handleButtonPress() {
  if (encoder.pressed()) {
    selectSetting(encoder.getPosition());
  }
}

void updateDisplay() {
  if (changeSetting) {
    displayChangeSettings(settings[selectedSetting]);
  } else {
    displayList(encoder.getPosition(), settingNames, settingsSize);
  }
}

void selectSetting(int i) {
  if (i == 0 && !changeSetting) {
    currentState = MAIN;
    settings_active = false;
    return;
  }
  if (!changeSetting) {
    selectedSetting = i;
    encoder.setPosition(settings[i].value / settings[i].step);
    changeSetting = true;
  } else {
    encoder.setPosition(selectedSetting);
    changeSetting = false;
  }
}

void loadSettings() {
}

