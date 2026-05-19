// state_hardware_test.cpp
// GRBL command tester. Rotate to pick command, press to send, see raw response.
// Hold 1s to exit back to MAIN.

#include "state_hardware_test.h"
#include <U8g2lib.h>
#include "logger.h"

#include <SdFat.h>
extern SdFat sd;

#define DBG_LOG_PATH "/hwtest.log"
static FsFile dbgLog;
static void dbg_log(const char* msg) {
  if (!dbgLog) return;
  dbgLog.println(msg);
  dbgLog.flush();
}

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
    // Open log file only once per test session
    if (dbgLog) dbgLog.close();
    sd.remove(DBG_LOG_PATH);
    dbgLog.open(DBG_LOG_PATH, O_WRONLY | O_CREAT);
    dbg_log("[DBG] Log file opened");
    hwtest_active = true;
    cmd_idx = 0;
    snprintf(cmd_resp, sizeof(cmd_resp), "Press to send");
    screen_dirty = true;
    encoder.setPosition(0);
    btn_down = false;
    dbg_log("[DBG] hwtest_active initialized");
  }

  if (screen_dirty) { screen_dirty = false; render(); }

  if (checkLongPress()) {
    dbg_log("[DBG] Exiting to MAIN via long press");
    hwtest_active = false;
    currentState = MAIN;
    if (dbgLog) { dbg_log("[DBG] Closing log"); dbgLog.close(); }
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
    char logbuf[64];
    snprintf(logbuf, sizeof(logbuf), "[DBG] Button pressed: idx=%d, cmd=%s", cmd_idx, cmd);
    dbg_log(logbuf);
    Log::send(cmd);
    dbg_log("[DBG] Logged command");
    Serial1.println(cmd);
    dbg_log("[DBG] Serial sent");
    unsigned long dl = millis() + 10000UL;
    char rbuf[64]; uint8_t rlen = 0;
    bool got = false;
    dbg_log("[DBG] Before while");
    while (millis() < dl) {
      if (!Serial1.available()) continue;
      char c = (char)Serial1.read();
      if (c == '\r') continue;
      if (c == '\n') {
        rbuf[rlen] = '\0';
        if (rlen > 0) {
          Log::recv(rbuf);
          snprintf(logbuf, sizeof(logbuf), "[DBG] Line: '%s'", rbuf);
          dbg_log(logbuf);
          snprintf(cmd_resp, sizeof(cmd_resp), "%s", rbuf);
          got = true;
          if (strncmp(rbuf, "ok",    2) == 0 ||
              strncmp(rbuf, "error", 5) == 0 ||
              strncmp(rbuf, "ALARM", 5) == 0) break;
        }
        rlen = 0;
      } else if (rlen < sizeof(rbuf) - 1) {
        rbuf[rlen++] = c;
      }
    }
    if (!got) {
      snprintf(logbuf, sizeof(logbuf), "[DBG] Response: TIMEOUT after 10s for cmd=%s", cmd);
      dbg_log(logbuf);
      snprintf(cmd_resp, sizeof(cmd_resp), "TIMEOUT");
    }
    screen_dirty = true;
    btn_down = false;
    dbg_log("[DBG] Button press handler finished");
  }
  //dbg_log("[DBG] Exiting state_hardware_test normally");
}
