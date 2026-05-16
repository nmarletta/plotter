// wifi_manager.h
// Handles WiFi station-mode connection (credentials from /wifi.cfg on SD)
// and a background HTTP server with the following endpoints:
//
//   GET  /           — HTML upload UI + GRBL terminal
//   GET  /files      — JSON array of gcode filenames on SD
//   POST /upload     — multipart/form-data file upload → writes to SD root
//   POST /grbl       — body: raw GRBL command → returns GRBL response text
//
// Call wifiBegin() once after SD is initialised.
// Call wifiTick() every loop iteration to service HTTP clients.

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

// Attempt to connect using /wifi.cfg. Returns true if connected.
// Reads ssid= and password= lines from the file.
bool wifiBegin();

// Service one pending HTTP client (non-blocking). Call every loop iteration.
void wifiTick();

// Returns the local IP as a string, or "" if not connected.
String wifiIP();

// Returns true if WiFi is currently connected.
bool wifiConnected();

#endif // WIFI_MANAGER_H
