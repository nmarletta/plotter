// state_hardware_test.cpp
// GRBL command tester. Rotate to pick command, press to send, see raw response.
// Hold 1s to exit back to MAIN.

#include "state_hardware_test.h"
#include <U8g2lib.h>
#include "logger.h"

extern U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2;
extern RotaryButton encoder;
extern State currentState;

#define LONG_PRESS_MS 1000UL

static bool hwtest_active = false;

static const char* const kCmds[] = { "M4 S800", "M5 S100", "$X" };
static const int kCmdCount = 3;
static int  cmd_idx  = 0;
static char cmd_resp[48] = "Press to send";
static bool screen_dirty = true;

static bool          btn_down    = false;
static unsigned long btn_down_ms = 0;

static bool checkLongPress() {
  bool down = (digitalRead(A6) == LOW);
  if (down && !btn_down) { btn_down = true; btn_down_ms = millis(); }
  else if (!down)         { btn_down = false; }
  if (btn_down && millis() - btn_down_ms >= LONG_PRESS_MS) { btn_down = false; return true; }
  return false;
}

static void render() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.setFontPosCenter();
  u8g2.setDrawColor(1);

  u8g2.setCursor(2, 11); u8g2.print("GRBL Test");
  char idxBuf[8]; snprintf(idxBuf, sizeof(idxBuf), "%d/%d", cmd_idx + 1, kCmdCount);
  u8g2.setCursor(128 - (int)strlen(idxBuf) * 6 - 2, 11);
  u8g2.print(idxBuf);
  u8g2.drawHLine(0, 17, 128);

  u8g2.setCursor(2, 30); u8g2.print(kCmds[cmd_idx]);
  u8g2.setCursor(2, 45); u8g2.print(cmd_resp);
  u8g2.setCursor(2, 58); u8g2.print("Press send | Hold exit");

  u8g2.sendBuffer();
}

void state_hardware_test() {
  if (!hwtest_active) {
    hwtest_active = true;
    cmd_idx = 0;
    snprintf(cmd_resp, sizeof(cmd_resp), "Press to send");
    screen_dirty = true;
    encoder.setPosition(0);
    btn_down = false;
  }

  if (screen_dirty) { screen_dirty = false; render(); }

  if (checkLongPress()) {
    hwtest_active = false;
    currentState = MAIN;
    return;
  }

  if (encoder.turned()) {
    encoder.constrainPosition(0, kCmdCount - 1);
    cmd_idx = encoder.getPosition();
    snprintf(cmd_resp, sizeof(cmd_resp), "Press to send");
    screen_dirty = true;
  }

  if (encoder.pressed()) {
    const char *cmd = kCmds[cmd_idx];
    Log::send(cmd);
    Serial1.println(cmd);

    unsigned long dl = millis() + 10000UL;
    char rbuf[64]; uint8_t rlen = 0;
    bool got = false;
    while (millis() < dl) {
      if (!Serial1.available()) continue;
      char c = (char)Serial1.read();
      if (c == '\r') continue;
      if (c == '\n') {
        rbuf[rlen] = '\0';
        if (rlen > 0) {
          Log::recv(rbuf);
          snprintf(cmd_resp, sizeof(cmd_resp), "%s", rbuf);
          got = true;
          if (strncmp(rbuf, "ok",    2) == 0 ||
              strncmp(rbuf, "error", 5) == 0 ||
              strncmp(rbuf, "ALARM", 5) == 0) break;
          rlen = 0;
        }
      } else if (rlen < sizeof(rbuf) - 1) {
        rbuf[rlen++] = c;
      }
    }
    if (!got) snprintf(cmd_resp, sizeof(cmd_resp), "TIMEOUT");
    screen_dirty = true;
    btn_down = false;
  }
}
