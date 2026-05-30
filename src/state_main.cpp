#include "state_main.h"

bool main_active = false;

String main_menuItems[] = { "Files", "Control", "GRBL Settings", "Pen Settings", "Diagnostics", "WiFi" };
int main_menuSize = 6;

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
      // GRBL Settings
      currentState = SETTINGS;
      break;

    case 3:
      // Pen Settings
      currentState = PEN_SETTINGS;
      break;

    case 4:
      // Diagnostics
      currentState = HWTEST;
      break;

    case 5:
      // WiFi status
      currentState = WIFI;
      break;
  }
}