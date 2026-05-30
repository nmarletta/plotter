#include "state_files.h"
#include <SdFat.h>

bool files_active = false;

String* files_menuItems = nullptr;

void state_files() {
  static int listSize;
  if (!files_active) {
    // Re-initialise the SD driver every time we enter the file list so that
    // a card that was removed and reinserted while the plotter is on is
    // detected correctly.  SdSpiConfig with SHARED_SPI is safe here because
    // the display uses the same SPI bus.
    sd.begin(SdSpiConfig(A5, SHARED_SPI, SD_SCK_HZ(400000)));
    delete[] files_menuItems;
    encoder.setPosition(0);
    fileHandler.loadFileNames();
    listSize = fileHandler.getFileCount() + 1;
    files_menuItems = new String[listSize];
    files_menuItems[0] = { "<- Back" };
    for (int i = 1; i < listSize; i++) {
      files_menuItems[i] = fileHandler.getFileName(i - 1);
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
