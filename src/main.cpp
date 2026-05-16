/*


*/
#include <Arduino.h>

// microSD card reader
#include <SdFat.h>
#include "FileHandler.h"
#define PIN_SDCARD_CS A5
SdFat sd;
FileHandler fileHandler;

// OLED display
#include <U8g2lib.h>
#define PIN_DISPLAY_SDA MOSI
#define PIN_DISPLAY_SCL SCK
#define PIN_DISPLAY_DC 6
#define PIN_DISPLAY_CS 5
#define PIN_DISPLAY_RES 7
U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, PIN_DISPLAY_CS, PIN_DISPLAY_DC, PIN_DISPLAY_RES);

// Rotary encoder
#include "RotaryButton.h"
#define PIN_ENCODER_CLK 1
#define PIN_ENCODER_DT 0
#define PIN_ENCODER_SW A6
RotaryButton encoder(PIN_ENCODER_DT, PIN_ENCODER_CLK, PIN_ENCODER_SW);

// Fan
#define PIN_FAN 2

#include "globals.h"
#include "wifi_manager.h"
#include "state_main.h"
#include "state_control.h"
#include "state_settings.h"
#include "state_pen_settings.h"
#include "state_files.h"
#include "state_plot.h"
#include "state_hardware_test.h"

void setup() {
  Serial.begin(250000);

  // SD init first — must happen before WiFiNINA takes over SPI
  encoder.begin();
  pinMode(PIN_SDCARD_CS, OUTPUT); digitalWrite(PIN_SDCARD_CS, HIGH);
  delay(100);
  sd.begin(SdSpiConfig(PIN_SDCARD_CS, SHARED_SPI, SD_SCK_HZ(400000)));

  u8g2.begin();
  pinMode(PIN_FAN, OUTPUT);
  serialMgr.begin(115200);
  loadPenCfg();

  // Connect to WiFi (reads /wifi.cfg from SD)
  wifiBegin();

  // Show WiFi status on OLED for 3 seconds
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.setFontPosCenter();
  if (wifiConnected()) {
    u8g2.setCursor(0, 20); u8g2.print("WiFi connected");
    u8g2.setCursor(0, 38); u8g2.print(wifiIP());
  } else {
    u8g2.setCursor(0, 32); u8g2.print("WiFi not connected");
  }
  u8g2.sendBuffer();
  delay(3000);

  currentState = MAIN;
}

void loop() {
  wifiTick(); // service any pending HTTP client
  switch (currentState) {
    case MAIN:
      state_main();
      break;
    case CONTROL:
      state_control();
      break;
    case SETTINGS:
      state_settings();
      break;
    case PEN_SETTINGS:
      state_pen_settings();
      break;
    case FILES:
      state_files();
      break;
    case PLOT:
      state_plot();
      break;
    case HWTEST:
      state_hardware_test();
      break;
  }
}
