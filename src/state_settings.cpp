#include "state_settings.h"

Setting settings[] = {
  { " ", "<- Back", "", -1, -1, -1, -1, -1 },
  { "$0", "Step Pulse Length", "µsec", -1, 0, 999, 0, 1 },
  { "$1", "Step Idle Delay", "msec", -1, 0, 999, 0, 10 },
  { "$2", "Step Port Invert", "mask", -1, 0, 7, 0, 1 },
  { "$3", "Dir. Port Invert", "mask", -1, 0, 7, 0, 1 },
  { "$4", "Step Enable Invert", "bool", -1, 0, 1, 0, 1 },
  { "$5", "Limit Pins Invert", "bool", -1, 0, 1, 0, 1 },
  { "$30", "Max Spindle Speed", "RPM", -1, 0, 9999, 0, 10 },
  { "$31", "Min Spindle Speed", "RPM", -1, 0, 1000, 0, 1 },
  { "$32", "Laser Mode", "bool", -1, 0, 1, 0, 1 },
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
};

const int settingsSize = sizeof(settings) / sizeof(settings[0]);


bool settings_active = false;
bool changeSetting = false;
int selectedSetting = 0;

void state_settings() {
  if (!settings_active) {
    encoder.setPosition(1);
    loadSettings();
    displayList(encoder.getPosition(), settings, settingsSize);
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
    displayList(encoder.getPosition(), settings, settingsSize);
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
    // Confirmed — send to GRBL
    if (settings[selectedSetting].command.startsWith("$")) {
      char buf[32];
      snprintf(buf, sizeof(buf), "%s=%g",
        settings[selectedSetting].command.c_str(),
        (double)settings[selectedSetting].value);
      Serial.print("SETTING >> "); Serial.println(buf);
      // Soft-reset GRBL first: puts it in STATE_ALARM where the hard-limit
      // ISR cannot fire, so our $xx=val command won't be wiped by mc_reset().
      serialMgr.softResetAndWait();
      bool ok = serialMgr.sendLine(buf);
      Serial.print("SETTING << "); Serial.println(ok ? "ok" : serialMgr.getLastResponse());
    }
    encoder.setPosition(selectedSetting);
    changeSetting = false;
  }
}

static void onGrblSetting(const String& cmd, float value) {
  for (int i = 1; i < settingsSize; i++) {
    if (settings[i].command == cmd) {
      settings[i].value = value;
      break;
    }
  }
}

void loadSettings() {
  // Set safe defaults first
  for (int i = 1; i < settingsSize; i++) settings[i].value = settings[i].minValue;

  // Soft-reset GRBL before sending $$ so it enters STATE_ALARM.
  // In STATE_ALARM the hard-limit pin-change ISR is suppressed, allowing
  // serial commands to be received without being wiped by mc_reset().
  serialMgr.softResetAndWait();

  // Load live values from GRBL
  serialMgr.querySettings(onGrblSetting);
}

