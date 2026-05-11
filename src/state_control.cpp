#include "state_control.h"

bool control_active = false;

String control_menuItems[] = { "<- Back", "Pen up", "Pen down", "X-position", "Y-position" };
int control_menuSize = 5;

bool changePosition = false;
int selectedControl = 0;

void state_control() {
  if (!control_active) {
    encoder.setPosition(0);
    displayList(encoder.getPosition(), control_menuItems, control_menuSize);
    control_active = true;
  }

  if (encoder.turned()) {
    encoder.constrainPosition(0, control_menuSize - 1);
    displayList(encoder.getPosition(), control_menuItems, control_menuSize);
  }

  if (encoder.pressed()) {
    control_menuSelect(encoder.getPosition());
    control_active = false;
  }
}

void control_menuSelect(int i) {
  if (!changePosition) {
    if (i == 0) {
      currentState = MAIN;
      control_active = false;
      return;
    } else if (i == 1 || i == 2) {
      // send coordinates
      return;
    } else if (i == 3 || i == 4) {
      selectedControl = i;
      changePosition = true;
      return;
    }
  } else {
    encoder.setPosition(selectedControl);
    changePosition = false;
  }
}