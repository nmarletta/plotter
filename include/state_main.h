#ifndef state_main_h
#define state_main_h

#include "globals.h"
#include "display.h"
#include "RotaryButton.h"
#include "state_hardware_test.h"

extern String main_menuItems[];
extern RotaryButton encoder;

void state_main();
void main_menuSelect(int encoderPosition);

#endif