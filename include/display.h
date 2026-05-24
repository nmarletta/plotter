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
void displayList(int currentItem, Setting list[], int size); // overload for settings menu
void displayPlot(const char* filename, int selected, float progress, bool paused, bool confirmCancel, bool error = false, bool resetting = false, int8_t alarmCode = 0);
void displayControl(int idx, int total, const char* item, const char* feedback);
void displayJog(char axis, float posMm, const char* feedback = nullptr);
void displayAlarm(int8_t alarmCode, int selected);
void displayChangeSettings(Setting setting);
void displayLine(int c, bool highlighted, const String& textq);
void displayScrollBar(int pos, int listSize);
void centeredText(String text, int posX, int posY);
#endif