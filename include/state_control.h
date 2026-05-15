#ifndef state_control_h
#define state_control_h

#include "globals.h"
#include "display.h"
#include "RotaryButton.h"
#include <Arduino.h>

extern Setting settings[];
extern RotaryButton encoder;

void state_control();
void control_menuSelect(int i);

#endif
