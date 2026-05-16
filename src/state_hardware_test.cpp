// state_hardware_test.cpp
// Hardware validation diagnostic state.
// Cycles through 5 sequential OLED screens, advancing on encoder press.
// Long-press (>1s) on any screen exits back to MAIN.
//
// Screen 1 — Display:  Visible = pass.
// Screen 2 — Encoder:  Live counter updated by rotation.
// Screen 3 — SD Card:  Lists root dir, shows file count or "SD FAIL".
// Screen 4 — GRBL:     Sends \n wake, shows first response or "NO GRBL".
// Screen 5 — Fan Pin:  Toggle PIN_FAN HIGH/LOW; long-press exits.

#include "state_hardware_test.h"
#include <U8g2lib.h>
#include "FileHandler.h"

// ---- Externs wired up in main.cpp / globals ----
extern U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2;
extern RotaryButton encoder;
extern FileHandler fileHandler;
extern SerialManager serialMgr;
extern State currentState;

#define PIN_FAN 2
#define HWTEST_SCREENS 5
#define LONG_PRESS_MS 1000UL

// ---- File-scope state ----
static bool hwtest_active  = false;
static int  hwtest_step    = 0;   // 0-based: 0=Display … 4=Fan
static bool screen_entered = false;
static bool screen_dirty   = true; // true = needs re-render

// Screen 2
static int  enc_counter    = 0;

// Screen 3
static char sd_result[24]  = "";

// Screen 4
static char grbl_result[24] = "";

// Screen 5
static bool fan_on         = false;

// Long-press tracking (any screen)
static bool     btn_down     = false;
static unsigned long btn_down_ms = 0;

// ---- Helpers ----

// Draw "N/5" step indicator in top-right corner
static void drawStepIndicator(int step) {
  char buf[6];
  snprintf(buf, sizeof(buf), "%d/5", step + 1);
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.setFontPosCenter();
  u8g2.setCursor(104, 11);
  u8g2.print(buf);
}

// Draw a horizontal divider line below the title row
static void drawTitleDivider() {
  u8g2.drawHLine(0, 17, 128);
}

// Detect long-press using millis() — call every loop iteration.
// Returns true once on the frame the long-press threshold is crossed.
static bool checkLongPress() {
  // Raw button state via RotaryButton::pressed() fires on release,
  // so we track the digital pin directly for hold duration.
  // RotaryButton SW is on A6 (pin index 20 on MKR).
  // We track via the encoder object: if it has been pressed but not yet
  // released we compare millis. Use Arduino digitalRead on A6 = 20.
  const uint8_t SW_PIN = A6;
  bool currently_down = (digitalRead(SW_PIN) == LOW);
  if (currently_down && !btn_down) {
    btn_down    = true;
    btn_down_ms = millis();
  } else if (!currently_down) {
    btn_down = false;
  }
  if (btn_down && (millis() - btn_down_ms >= LONG_PRESS_MS)) {
    btn_down = false; // consume
    return true;
  }
  return false;
}

static void exitToMain() {
  hwtest_active  = false;
  hwtest_step    = 0;
  screen_entered = false;
  enc_counter    = 0;
  fan_on         = false;
  digitalWrite(PIN_FAN, LOW); // ensure fan off on exit
  currentState   = MAIN;
}

// ---- Screen renderers ----

static void renderScreen1() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.setFontPosCenter();
  u8g2.setCursor(0, 11); u8g2.print("Display");
  drawStepIndicator(0);
  drawTitleDivider();
  u8g2.setCursor(0, 33); u8g2.print("Display OK");
  u8g2.setCursor(0, 52); u8g2.print("Press -> to continue");
  u8g2.sendBuffer();
}

static void renderScreen2() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.setFontPosCenter();
  u8g2.setCursor(0, 11); u8g2.print("Encoder");
  drawStepIndicator(1);
  drawTitleDivider();
  char buf[20];
  snprintf(buf, sizeof(buf), "Count: %d", enc_counter);
  u8g2.setCursor(0, 33); u8g2.print(buf);
  u8g2.setCursor(0, 52); u8g2.print("Press to confirm");
  u8g2.sendBuffer();
}

static void renderScreen3() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.setFontPosCenter();
  u8g2.setCursor(0, 11); u8g2.print("SD Card");
  drawStepIndicator(2);
  drawTitleDivider();
  u8g2.setCursor(0, 33); u8g2.print(sd_result);
  u8g2.setCursor(0, 52); u8g2.print("Press -> to continue");
  u8g2.sendBuffer();
}

static void renderScreen4() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.setFontPosCenter();
  u8g2.setCursor(0, 11); u8g2.print("GRBL Serial");
  drawStepIndicator(3);
  drawTitleDivider();
  u8g2.setCursor(0, 33); u8g2.print(grbl_result);
  u8g2.setCursor(0, 52); u8g2.print("Press -> to continue");
  u8g2.sendBuffer();
}

static void renderScreen5() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.setFontPosCenter();
  u8g2.setCursor(0, 11); u8g2.print("Fan Pin");
  drawStepIndicator(4);
  drawTitleDivider();
  u8g2.setCursor(0, 33); u8g2.print(fan_on ? "FAN ON" : "FAN OFF");
  u8g2.setCursor(0, 46); u8g2.print("Press: toggle");
  u8g2.setCursor(0, 59); u8g2.print("Hold 1s: exit");
  u8g2.sendBuffer();
}

// ---- Entry actions (run once per screen) ----

static void enterScreen3() {
  fileHandler.loadFileNames();
  int count = fileHandler.getFileCount();
  if (count >= 0) {
    snprintf(sd_result, sizeof(sd_result), "SD OK: %d files", count);
  } else {
    snprintf(sd_result, sizeof(sd_result), "SD FAIL");
  }
}

static void enterScreen4() {
  // Flush any stale bytes
  while (Serial1.available()) Serial1.read();
  // Send empty line to wake GRBL
  Serial1.println("");

  unsigned long start = millis();
  String line = "";
  bool found = false;
  while (millis() - start < 2000UL) {
    if (Serial1.available()) {
      line = Serial1.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) {
        found = true;
        break;
      }
    }
  }
  if (found) {
    line.toCharArray(grbl_result, sizeof(grbl_result));
  } else {
    snprintf(grbl_result, sizeof(grbl_result), "NO GRBL");
  }
}

// ---- Main state function ----

void state_hardware_test() {
  // On entry into this state
  if (!hwtest_active) {
    hwtest_active  = true;
    hwtest_step    = 0;
    screen_entered = false;
    enc_counter    = 0;
    fan_on         = false;
    encoder.setPosition(0);
  }

  // Run entry action once per screen
  if (!screen_entered) {
    screen_entered = true;
    screen_dirty   = true;
    switch (hwtest_step) {
      case 2: enterScreen3(); break;
      case 3: enterScreen4(); btn_down = false; break; // clear stale long-press state after 2s blocking wait
      default: break;
    }
  }

  // Render current screen only when dirty
  if (screen_dirty) {
    screen_dirty = false;
    switch (hwtest_step) {
      case 0: renderScreen1(); break;
      case 1: renderScreen2(); break;
      case 2: renderScreen3(); break;
      case 3: renderScreen4(); break;
      case 4: renderScreen5(); break;
    }
  }

  // Long-press → exit to MAIN from any screen
  if (checkLongPress()) {
    exitToMain();
    return;
  }

  // Screen 2: update counter on rotation
  if (hwtest_step == 1 && encoder.turned()) {
    enc_counter += encoder.getPosition() > 0 ? 1 : -1;
    encoder.setPosition(0);
    screen_dirty = true;
  }

  // Screen 5: short press toggles fan
  if (hwtest_step == 4 && encoder.pressed()) {
    fan_on = !fan_on;
    digitalWrite(PIN_FAN, fan_on ? HIGH : LOW);
    screen_dirty = true;
    btn_down = false; // restart long-press window so holding after a tap doesn't exit
    return;
  }

  // All other screens: short press advances to next screen
  if (hwtest_step != 4 && encoder.pressed()) {
    hwtest_step++;
    screen_entered = false;
    screen_dirty   = true;
    encoder.setPosition(0);
  }
}
