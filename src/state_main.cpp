#include "state_main.h"

bool main_active = false;

String main_menuItems[] = { "Files", "Control", "Settings", "Diagnostics" };
int main_menuSize = 4;

void state_main() {
  if (!main_active) {
    encoder.setPosition(0);
    displayList(encoder.getPosition(), main_menuItems, main_menuSize);
    main_active = true;
  }

  if (encoder.turned()) {
    encoder.constrainPosition(0, main_menuSize - 1);
    displayList(encoder.getPosition(), main_menuItems, main_menuSize);
  }

  if (encoder.pressed()) {
    main_menuSelect(encoder.getPosition());
    main_active = false;
  }
}

void main_menuSelect(int i) {
  switch (i) {
    case 0:
      // Plot
      currentState = FILES;
      break;

    case 1:
      // Control
      currentState = CONTROL;
      break;

    case 2:
      // Settings
      currentState = SETTINGS;
      break;

    case 3:
      // Diagnostics
      currentState = HWTEST;
      break;
  }
}