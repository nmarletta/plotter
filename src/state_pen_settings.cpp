// state_pen_settings.cpp
// Controller-local pen settings: Pen Down S, Pen Up S, Overwrite S.
// Saved to /pen.cfg on SD; loaded on boot via loadPenCfg().
//
// When Overwrite S = 1, the gcode streamer replaces M3 S values with
// g_penDownS, and M5 with M3 S{g_penUpS}, regardless of what the file says.

#include "state_pen_settings.h"
#include <SdFat.h>
#include <U8g2lib.h>

extern SdFat sd;
extern U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2;
extern RotaryButton encoder;

#define PEN_CFG_PATH "/pen.cfg"

static Setting penSettings[] = {
  { " ",  "<- Back",     "",  -1,    0,    0,  0,   1 },
  { "pd", "Pen Down S",  "",  700,   0, 1000,  0,  10 },
  { "pu", "Pen Up S",    "",  100,   0, 1000,  0,  10 },
  { "ow", "Overwrite S", "",    0,   0,    1,  0,   1 },
};
const int penSettingsSize = sizeof(penSettings) / sizeof(penSettings[0]);

static bool ps_active        = false;
static bool ps_changeSetting = false;
static int  ps_selected      = 1;

// ---- SD persistence ----

static void savePenCfg() {
  sd.remove(PEN_CFG_PATH);
  FsFile f;
  if (!f.open(PEN_CFG_PATH, O_WRONLY | O_CREAT)) return;
  char buf[32];
  snprintf(buf, sizeof(buf), "penDown=%d\n",  (int)penSettings[1].value); f.print(buf);
  snprintf(buf, sizeof(buf), "penUp=%d\n",    (int)penSettings[2].value); f.print(buf);
  snprintf(buf, sizeof(buf), "overwrite=%d\n",(int)penSettings[3].value); f.print(buf);
  f.close();
}

void loadPenCfg() {
  FsFile f;
  if (!f.open(PEN_CFG_PATH, O_READ)) {
    // No file — keep compiled-in defaults and publish to globals
    g_penDownS   = (int)penSettings[1].value;
    g_penUpS     = (int)penSettings[2].value;
    g_overwriteS = (bool)penSettings[3].value;
    return;
  }
  char line[32];
  uint8_t len = 0;
  while (f.available()) {
    char c = (char)f.read();
    if (c == '\r') continue;
    bool eol = (c == '\n') || !f.available();
    if (!f.available() && c != '\n') { if (len < sizeof(line) - 1) line[len++] = c; }
    if (eol) {
      line[len] = '\0';
      char *eq = strchr(line, '=');
      if (eq) {
        *eq = '\0';
        int val = atoi(eq + 1);
        if      (strcmp(line, "penDown")   == 0) penSettings[1].value = val;
        else if (strcmp(line, "penUp")     == 0) penSettings[2].value = val;
        else if (strcmp(line, "overwrite") == 0) penSettings[3].value = val;
      }
      len = 0;
    } else {
      if (len < sizeof(line) - 1) line[len++] = c;
    }
  }
  f.close();
  g_penDownS   = (int)penSettings[1].value;
  g_penUpS     = (int)penSettings[2].value;
  g_overwriteS = (bool)penSettings[3].value;
}

// ---- State function ----

void state_pen_settings() {
  if (!ps_active) {
    loadPenCfg();
    ps_selected      = 1;
    ps_changeSetting = false;
    encoder.setPosition(1);
    displayList(encoder.getPosition(), penSettings, penSettingsSize);
    ps_active = true;
  }

  if (encoder.turned()) {
    if (ps_changeSetting) {
      encoder.constrainPosition(
        (int)(penSettings[ps_selected].minValue / penSettings[ps_selected].step),
        (int)(penSettings[ps_selected].maxValue / penSettings[ps_selected].step));
      penSettings[ps_selected].value = encoder.getPosition() * penSettings[ps_selected].step;
      displayChangeSettings(penSettings[ps_selected]);
    } else {
      encoder.constrainPosition(0, penSettingsSize - 1);
      ps_selected = encoder.getPosition();
      displayList(encoder.getPosition(), penSettings, penSettingsSize);
    }
  }

  if (encoder.pressed()) {
    if (ps_selected == 0 && !ps_changeSetting) {
      // Back — save and exit
      savePenCfg();
      g_penDownS   = (int)penSettings[1].value;
      g_penUpS     = (int)penSettings[2].value;
      g_overwriteS = (bool)penSettings[3].value;
      ps_active    = false;
      currentState = MAIN;
      return;
    }
    if (!ps_changeSetting) {
      ps_selected = encoder.getPosition();
      encoder.setPosition((int)(penSettings[ps_selected].value / penSettings[ps_selected].step));
      ps_changeSetting = true;
      displayChangeSettings(penSettings[ps_selected]);
    } else {
      // Confirmed — update globals immediately and save
      g_penDownS   = (int)penSettings[1].value;
      g_penUpS     = (int)penSettings[2].value;
      g_overwriteS = (bool)penSettings[3].value;
      savePenCfg();
      encoder.setPosition(ps_selected);
      ps_changeSetting = false;
      displayList(encoder.getPosition(), penSettings, penSettingsSize);
    }
  }
}
