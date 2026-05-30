// state_wifi.cpp
// WiFi status screen: shows SSID, connected/disconnected, IP address.
// When disconnected: [Reconnect] [Back]. When connected: [Back].

#include "state_wifi.h"
#include "wifi_manager.h"
#include <U8g2lib.h>

extern U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2;
extern RotaryButton encoder;
extern State currentState;

static bool wifi_active = false;
static int  wifi_sel    = 0;

static void drawWifi() {
    bool conn = wifiConnected();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);

    u8g2.drawStr(0, 10, "WiFi");
    u8g2.drawHLine(0, 12, 128);

    // SSID
    char line[40];
    const char* ssid = wifiSSID();
    snprintf(line, sizeof(line), "SSID: %s", ssid[0] ? ssid : "(none)");
    u8g2.drawStr(0, 25, line);

    // Status
    u8g2.drawStr(0, 38, conn ? "Connected" : "Disconnected");

    // IP when connected
    if (conn) {
        String ip = wifiIP();
        u8g2.drawStr(0, 51, ip.c_str());
    }

    // Bottom button row
    if (conn) {
        bool sel = (wifi_sel == 0);
        if (sel) { u8g2.drawBox(0, 54, 40, 10); u8g2.setDrawColor(0); }
        u8g2.drawStr(4, 63, "Back");
        u8g2.setDrawColor(1);
    } else {
        // [Reconnect]  [Back]
        bool selR = (wifi_sel == 0);
        bool selB = (wifi_sel == 1);
        if (selR) { u8g2.drawBox(0, 54, 66, 10); u8g2.setDrawColor(0); }
        u8g2.drawStr(4, 63, "Reconnect");
        u8g2.setDrawColor(1);
        if (selB) { u8g2.drawBox(70, 54, 58, 10); u8g2.setDrawColor(0); }
        u8g2.drawStr(74, 63, "Back");
        u8g2.setDrawColor(1);
    }

    u8g2.sendBuffer();
}

static void showConnecting() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, "WiFi");
    u8g2.drawHLine(0, 12, 128);
    u8g2.drawStr(0, 32, "Connecting...");
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
            // Reconnect: block while connecting (up to 15 s), show progress
            showConnecting();
            wifiBegin();
            wifi_sel = 0;
            encoder.setPosition(0);
            encoder.constrainPosition(0, wifiConnected() ? 0 : 1);
            drawWifi();
        }
    }
}
