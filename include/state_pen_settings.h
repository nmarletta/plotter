// state_pen_settings.h
// Local controller pen settings: pen down S value, pen up S value, overwrite mode.
// Values are persisted to /pen.cfg on SD card.

#ifndef STATE_PEN_SETTINGS_H
#define STATE_PEN_SETTINGS_H

#include "globals.h"
#include "display.h"
#include "RotaryButton.h"

void state_pen_settings();
void loadPenCfg();  // call from setup() to restore saved values on boot

#endif // STATE_PEN_SETTINGS_H
