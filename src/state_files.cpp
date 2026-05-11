#include "state_files.h"

bool files_active = false;

String* files_menuItems = nullptr;

void state_files() {
  static int listSize;
  if (!files_active) {
    delete[] files_menuItems; // Free previous allocation
    encoder.setPosition(0);
    fileHandler.loadFileNames();
    // Serial.println(fileHandler.checkAccess());
    listSize = fileHandler.getFileCount() + 1;
    files_menuItems = new String[listSize];
    files_menuItems[0] = { "<- Back" };
    for (int i = 1; i < listSize; i++) {
      files_menuItems[i] = fileHandler.getFileName(i - 1);  // Adjust index for file names
      Serial.println(files_menuItems[i]);
    }
    displayList(encoder.getPosition(), files_menuItems, listSize);
    files_active = true;
  }

  if (encoder.turned()) {
    encoder.constrainPosition(0, listSize - 1);
    displayList(encoder.getPosition(), files_menuItems, listSize);
  }

  if (encoder.pressed()) {
    files_menuSelect(encoder.getPosition());
  }
}

// Example functions for menu actions
void files_menuSelect(int i) {
  if (i == 0) {
    currentState = MAIN;
    files_active = false;
  } else {
    selectedFile = fileHandler.getFileName(i - 1);
    currentState = PLOT;
    files_active = false;
  }
}
