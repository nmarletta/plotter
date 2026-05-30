// state_wifi.cpp
// WiFi status screen: shows SSID, connected/disconnected, IP address.
// When disconnected: [Reconnect] | [Back]. When connected: [Back].

#include "state_wifi.h"
#include "wifi_manager.h"
#include "display.h"
#include <U8g2lib.h>

extern U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2;
extern RotaryButton encoder;
extern State currentState;

static bool wifi_active = false;
static int  wifi_sel    = 0;

static void drawWifi() {
    bool conn = wifiConnected();
    const char* ssid = wifiSSID();

    u8g2.clearBuffer();
    u8g2.setDrawColor(1);

    // ---- Title bar (y=0–20, cy=11) ----
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.setFontPosCenter();
    u8g2.setCursor(4, 11);
    u8g2.print("WiFi");
    const char* statusStr = conn ? "Connected" : "No signal";
    u8g2.setCursor(128 - (int)strlen(statusStr) * 6 - 4, 11);
    u8g2.print(statusStr);
    u8g2.drawHLine(0, 20, 128);

    // ---- Content area (y=21–42) ----
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setFontPosTop();
    char ssidLine[32];
    snprintf(ssidLine, sizeof(ssidLine), "SSID: %s", ssid[0] ? ssid : "(none)");
    u8g2.setCursor(4, 23);
    u8g2.print(ssidLine);
    if (conn) {
        u8g2.setCursor(4, 35);
        u8g2.print("plotter.local");
    }

    // ---- Button row (y=43–63, cy=53) — same layout as displayPlot ----
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.setFontPosCenter();

    if (conn) {
        if (wifi_sel == 0) { u8g2.drawBox(32, 43, 64, 21); u8g2.setDrawColor(0); }
        centeredText("Back", 64, 53);
        u8g2.setDrawColor(1);
    } else {
        if (wifi_sel == 0) { u8g2.drawBox(0, 43, 63, 21); u8g2.setDrawColor(0); }
        else u8g2.setDrawColor(1);
        centeredText("Reconnect", 32, 53);
        u8g2.setDrawColor(1);
        u8g2.drawVLine(63, 43, 21);
        if (wifi_sel == 1) { u8g2.drawBox(64, 43, 64, 21); u8g2.setDrawColor(0); }
        else u8g2.setDrawColor(1);
        centeredText("Back", 96, 53);
        u8g2.setDrawColor(1);
    }

    u8g2.sendBuffer();
}

static void showConnecting() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.setFontPosCenter();
    u8g2.setDrawColor(1);
    u8g2.setCursor(4, 11);
    u8g2.print("WiFi");
    u8g2.drawHLine(0, 20, 128);
    centeredText("Connecting...", 64, 32);
    u8g2.sendBuffer();
}

void state_wifi() {
    if (!wifi_active) {
        wifi_sel = 0;
        encoder.setPosition(0);
        encoder.constrainPosition(0, wifiConnected() ? 0 : 1);
        drawWifi();
        wifi_active = true;
    }

    if (encoder.turned()) {
        encoder.constrainPosition(0, wifiConnected() ? 0 : 1);
        wifi_sel = encoder.getPosition();
        drawWifi();
    }

    if (encoder.pressed()) {
        bool conn = wifiConnected();
        if (conn || wifi_sel == 1) {
            wifi_active  = false;
            currentState = MAIN;
        } else {
            showConnecting();
            wifiBegin();
            wifi_sel = 0;
            encoder.setPosition(0);
            encoder.constrainPosition(0, wifiConnected() ? 0 : 1);
            drawWifi();
        }
    }
}
