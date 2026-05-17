#include "display.h"

extern U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2;

DisplayContainers containers[] = {
  { 0, 0, 124, 21, 62, 11 },
  { 0, 21, 124, 22, 62, 32 },
  { 0, 43, 124, 21, 62, 53 },
};

void displayList(int selected, Setting list[], int size) {
  String names[size];
  for (int i = 0; i < size; i++) names[i] = list[i].name;
  displayList(selected, names, size);
}

void displayList(int selected, String list[], int size) {
  int startIndex;
  if (selected == 0) {
    startIndex = selected;
  } else if (selected == size - 1) {
    startIndex = selected - 2;
  } else {
    startIndex = selected - 1;
  }
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.setFontPosCenter();


  for (int i = 0; i < 3; i++) {
    int displayIndex = startIndex + i;
    if (displayIndex < size) {
      bool highlighted = displayIndex == selected ? true : false;
      displayLine(i, highlighted, list[displayIndex]);
      // Serial.println("a");
      // Serial.println(list[displayIndex]);
      // Serial.println("b");
    }
  }

  if (size > 3) {
    displayScrollBar(selected, size);
  }
  u8g2.sendBuffer();
}

void displayLine(int c, bool highlighted, const String& textq) {
  if (highlighted) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(containers[c].x, containers[c].y, containers[c].w, containers[c].h);
    u8g2.setDrawColor(0);
  } else {
    u8g2.setDrawColor(1);
  }
  u8g2.setCursor(4, containers[c].cy + 1);
  u8g2.print(textq);
}

void displayJog(char axis, float posX, float posY) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.setFontPosCenter();
  u8g2.setDrawColor(1);

  // Row 0: axis label
  char title[8];
  snprintf(title, sizeof(title), "Jog %c", axis);
  centeredText(title, 64, 8);

  // Row 1: X and Y position
  char posBuf[24];
  snprintf(posBuf, sizeof(posBuf), "X:%.1f  Y:%.1f", (double)posX, (double)posY);
  centeredText(posBuf, 64, 28);

  // Row 2: full-width Done button
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 43, 128, 21);
  u8g2.setDrawColor(0);
  centeredText("Done", 64, 53);
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

void displayAlarm(int8_t alarmCode, int selected) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.setDrawColor(1);
  u8g2.setFontPosCenter();

  // Row 0: title
  centeredText("ALARM", 64, 11);

  // Row 1: alarm description
  char alarmBuf[20];
  if      (alarmCode == 0)  strcpy(alarmBuf, "! Locked");
  else if (alarmCode == 1)  strcpy(alarmBuf, "! Hard Limit");
  else if (alarmCode == 2)  strcpy(alarmBuf, "! Soft Limit");
  else if (alarmCode == 3)  strcpy(alarmBuf, "! Reset/Motion");
  else if (alarmCode == 7)  strcpy(alarmBuf, "! Homing: no sw");
  else if (alarmCode == 8)  strcpy(alarmBuf, "! Homing: failed");
  else if (alarmCode == 9)  strcpy(alarmBuf, "! Homing: pulloff");
  else if (alarmCode == -1) strcpy(alarmBuf, "! GRBL reset");
  else if (alarmCode == -2) strcpy(alarmBuf, "! No response");
  else                      snprintf(alarmBuf, sizeof(alarmBuf), "! Alarm: %d", alarmCode);
  centeredText(alarmBuf, 64, 32);

  // Row 2: Unlock / Reset buttons
  if (selected == 0) {
    u8g2.setDrawColor(1); u8g2.drawBox(0, 43, 63, 21); u8g2.setDrawColor(0);
  } else {
    u8g2.setDrawColor(1);
  }
  centeredText("Unlock", 32, 53);

  u8g2.setDrawColor(1);
  u8g2.drawVLine(64, 43, 21);

  if (selected == 1) {
    u8g2.setDrawColor(1); u8g2.drawBox(65, 43, 63, 21); u8g2.setDrawColor(0);
  } else {
    u8g2.setDrawColor(1);
  }
  centeredText("Reset", 96, 53);

  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

void displayChangeSettings(Setting setting) {
  u8g2.setDrawColor(1);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_10x20_tf);
  u8g2.setFontPosCenter();

  char floatStr[15];
  snprintf(floatStr, sizeof(floatStr), "%.*f", setting.decimals, (double)setting.value);
  String value = String(floatStr);
  centeredText(setting.command + " = " + value, 64, 32);
  centeredText(setting.unit, 64, 48);
  u8g2.sendBuffer();
}

void centeredText(String text, int posX, int posY) {
  int textWidth = u8g2.getStrWidth(text.c_str());  // Convert to C-style string
  int x = ((posX * 2) - textWidth) / 2;
  u8g2.setCursor(x, posY);
  u8g2.print(text);
}

void displayScrollBar(int pos, int listSize) {
  u8g2.setDrawColor(1);
  for (int i = 0; i < 64; i += 2) {
    u8g2.drawPixel(126, i);
    // u8g2.drawPixel(125, i+1);
    // u8g2.drawPixel(127, i+1);
  }
  int h = (64 / (listSize)) * 3;
  int y = map(pos, 0, listSize - 1, 0, 63 - h);
  u8g2.drawBox(125, y, 3, h);
}

void displayPlot(const char* filename, int selected, float progress, bool paused, bool confirmCancel, bool error, bool resetting, int8_t alarmCode) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();

  // Row 0: filename left, progress % right (always visible)
  char truncName[18];
  size_t fnLen = strlen(filename);
  if (fnLen > 17) {
    memcpy(truncName, filename, 14);
    truncName[14] = '.';
    truncName[15] = '.';
    truncName[16] = '.';
    truncName[17] = '\0';
  } else {
    strcpy(truncName, filename);
  }
  u8g2.setCursor(2, 1);
  u8g2.print(truncName);

  char pctBuf[6];
  snprintf(pctBuf, sizeof(pctBuf), "%d%%", (int)(progress * 100.0f));
  u8g2.setCursor(128 - (int)strlen(pctBuf) * 6 - 2, 1);
  u8g2.print(pctBuf);

  // Alarm state: full-screen alarm layout
  if (alarmCode > 0) {
    // Row 1: alarm description
    u8g2.setFontPosCenter();
    char alarmBuf[20];
    if (alarmCode == 1)      strcpy(alarmBuf, "! Hard Limit");
    else if (alarmCode == 2) strcpy(alarmBuf, "! Soft Limit");
    else if (alarmCode == 3) strcpy(alarmBuf, "! Reset/Motion");
    else if (alarmCode == 7) strcpy(alarmBuf, "! Homing: no sw");
    else if (alarmCode == 8) strcpy(alarmBuf, "! Homing: failed");
    else if (alarmCode == 9) strcpy(alarmBuf, "! Homing: pulloff");
    else                     snprintf(alarmBuf, sizeof(alarmBuf), "! Alarm: %d", alarmCode);
    centeredText(alarmBuf, 64, 28);

    // Row 2: Unlock / Reset buttons
    u8g2.setFontPosCenter();
    if (selected == 0) {
      u8g2.setDrawColor(1); u8g2.drawBox(0, 43, 63, 21); u8g2.setDrawColor(0);
    } else {
      u8g2.setDrawColor(1);
    }
    centeredText("Unlock", 32, 53);
    u8g2.setDrawColor(1);
    u8g2.drawVLine(64, 43, 21);
    if (selected == 1) {
      u8g2.setDrawColor(1); u8g2.drawBox(65, 43, 63, 21); u8g2.setDrawColor(0);
    } else {
      u8g2.setDrawColor(1);
    }
    centeredText("Reset", 96, 53);
    u8g2.setDrawColor(1);
    u8g2.sendBuffer();
    return;
  }

  // Row 1: progress bar, or status message when error/resetting
  if (error) {
    u8g2.setFontPosCenter();
    centeredText("! GRBL Error", 64, 24);
  } else if (resetting) {
    u8g2.setFontPosCenter();
    centeredText("Resetting...", 64, 24);
  } else {
    u8g2.drawFrame(2, 18, 124, 11);
    int barW = (int)(progress * 120.0f);
    if (barW > 0) u8g2.drawBox(4, 20, barW, 7);
  }

  // Row 2: two-button menu at bottom
  const char* label0 = error   ? "Retry"
                     : resetting ? "..."
                     : paused    ? "Continue"
                     :             "Pause";
  const char* label1 = confirmCancel ? "Confirm?" : "Cancel";

  u8g2.setFontPosCenter();

  // Left button (Pause/Continue)
  if (selected == 0) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 43, 63, 21);
    u8g2.setDrawColor(0);
  } else {
    u8g2.setDrawColor(1);
  }
  centeredText(String(label0), 32, 53);

  // Divider
  u8g2.setDrawColor(1);
  u8g2.drawVLine(64, 43, 21);

  // Right button (Cancel/Confirm)
  if (selected == 1) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(65, 43, 63, 21);
    u8g2.setDrawColor(0);
  } else {
    u8g2.setDrawColor(1);
  }
  centeredText(String(label1), 96, 53);

  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}
