#ifndef display_h
#define display_h

#include <Arduino.h>
#include <U8g2lib.h>
#include "globals.h"

struct DisplayContainers {
  int x;
  int y;
  int w;
  int h;
  int cx;
  int cy;
};

void displayList(int currentItem, String list[], int size);
void displayPlot(const char* filename, int selected, float progress, bool paused, bool confirmCancel);
void displayChangeSettings(Setting setting);
void displayLine(int c, bool highlighted, const String& textq);
void displayScrollBar(int pos, int listSize);
void centeredText(String text, int posX, int posY);
#endif