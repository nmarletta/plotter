#ifndef state_settings_h
#define state_settings_h

#include "globals.h"
#include "display.h"
#include "RotaryButton.h"

extern Setting settings[];
extern RotaryButton encoder;

void state_settings();
void selectSetting(int i);
void loadSettings();
void handleEncoderChange();
void handleButtonPress();
void updateDisplay();

#endif
