#ifndef state_files_h
#define state_files_h

#include <U8g2lib.h>
#include "globals.h"
#include "display.h"
#include "fileHandler.h"
#include "RotaryButton.h"

extern RotaryButton encoder;
extern FileHandler fileHandler;

void state_files();
void files_menuSelect(int encoderPosition);

#endif