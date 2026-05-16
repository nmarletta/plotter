#include "state_control.h"
#include "logger.h"

static const int  kJogIntervalMs  = 80;   // ms to accumulate encoder ticks before sending
static const int  kJogFeed        = 1000; // mm/min
static const int  kJogStepMm      = 1;    // mm per encoder tick
static const int  kDisplayMs      = 100;  // ms between jog display updates
static const int  kCancelTimeoutMs = 500; // ms before giving up on cancel ok

bool control_active = false;

String control_menuItems[] = { "<- Back", "Pen Up", "Pen Down", "Jog X", "Jog Y" };
int    control_menuSize    = 5;

static int           _selectedControl = 0;
static bool          _jogging         = false;
static int           _jogAxis         = 0;
static int           _jogAccum        = 0;
static unsigned long _lastJogMs       = 0;
static unsigned long _lastDisplayMs   = 0;
static unsigned long _cancelStartMs   = 0; // when _cancelPending was set
static float         _dispPos         = 0; // local mm position for display
static char          _grblLine[64];
static uint8_t       _grblLineLen     = 0;
static bool          _jogAlarm        = false;
static bool          _pendingJog      = false;
static bool          _cancelPending   = false;
static int           _lastJogSign     = 0;

// Flush any bytes sitting in the Serial1 buffer.
static void flushGrbl() {
  delay(50);
  while (Serial1.available()) Serial1.read();
}

// Send a pen command and wait for GRBL's ok.
// grbl-servo fork: M5 Sxxx = pen down, M4 Sxxx = pen up.
static void sendPen(int sValue, bool isUp, const char *label) {
  Log::nav(label);
  flushGrbl();
  char buf[16];
  if (isUp) {
    snprintf(buf, sizeof(buf), "M4 S%d", sValue);
  } else {
    snprintf(buf, sizeof(buf), "M5 S%d", sValue);
  }
  Log::send(buf);
  Serial1.println(buf);
  bool ok = serialMgr.waitForOk(1000);
  while (Serial1.available()) Serial1.read();
  if (!ok) Log::err(serialMgr.getLastResponse().c_str());
}

// Read pending GRBL bytes; parse ok/alarm.
// No ? polling here — SPI display updates drop UART bytes on SAMD21.
static void pollGrbl() {
  while (Serial1.available()) {
    char c = (char)Serial1.read();
    if (c == '\r') continue;
    if (c == '\n') {
      _grblLine[_grblLineLen] = '\0';
      if (_grblLineLen > 0) {
        if (_grblLine[0] != '<') Log::recv(_grblLine); // skip status reports
        if (strncmp(_grblLine, "ALARM:", 6) == 0) _jogAlarm = true;
        if (strncmp(_grblLine, "Grbl ",  5) == 0) _jogAlarm = true;
        if (strcmp(_grblLine, "ok") == 0) { _pendingJog = false; _cancelPending = false; }
      }
      _grblLineLen = 0;
    } else if (_grblLineLen < sizeof(_grblLine) - 1) {
      _grblLine[_grblLineLen++] = c;
    }
  }
}

void state_control() {
  if (!control_active) {
    _jogging = false; _jogAccum = 0;
    encoder.setPosition(0);
    flushGrbl();
    Log::nav("entered control");
    Log::send("$X");
    Serial1.println("$X");
    serialMgr.waitForOk(300);
    flushGrbl();
    displayList(encoder.getPosition(), control_menuItems, control_menuSize);
    control_active = true;
  }

  if (_jogging) {
    pollGrbl();

    if (_jogAlarm) {
      Log::nav("JOG: alarm, exiting");
      Log::sendByte(0x85);
      Serial1.write(0x85);
      while (Serial1.available()) Serial1.read();
      _jogging = false; _jogAlarm = false; _pendingJog = false;
      _cancelPending = false; _lastJogSign = 0;
      encoder.setPosition(_selectedControl);
      displayList(encoder.getPosition(), control_menuItems, control_menuSize);
      return;
    }

    if (encoder.turned()) {
      int delta = encoder.getPosition() * kJogStepMm;
      _jogAccum += delta;
      _dispPos  += delta;
      encoder.setPosition(0);
    }

    // Update display — no ? polling, use local tracked position
    if (millis() - _lastDisplayMs >= kDisplayMs) {
      _lastDisplayMs = millis();
      displayJog(_jogAxis == 0 ? 'X' : 'Y', _dispPos, _dispPos);
    }

    // Safety: if cancel ok never arrives, unblock after timeout
    if (_cancelPending && millis() - _cancelStartMs > kCancelTimeoutMs) {
      Log::err("JOG: cancel timeout, unblocking");
      _cancelPending = false; _pendingJog = false;
    }

    if (millis() - _lastJogMs >= kJogIntervalMs && _jogAccum != 0 && !_cancelPending) {
      int newSign = (_jogAccum > 0) ? 1 : -1;
      if (_pendingJog && newSign != _lastJogSign) {
        Log::sendByte(0x85);
        Serial1.write(0x85); _pendingJog = false; _cancelPending = true; _cancelStartMs = millis();
        Log::nav("JOG: cancel (direction change)");
      }
      if (!_pendingJog && !_cancelPending) {
        char buf[40];
        snprintf(buf, sizeof(buf), "$J=G21G91%c%dF%d", _jogAxis == 0 ? 'X' : 'Y', _jogAccum, kJogFeed);
        Log::send(buf);
        Serial1.println(buf);
        _pendingJog = true; _lastJogSign = newSign; _jogAccum = 0; _lastJogMs = millis();
      }
    }

    if (encoder.pressed()) {
      if (_pendingJog) {
        // Only send cancel if a jog is actually in-flight; GRBL ignores 0x85 when idle
        Log::sendByte(0x85);
        Serial1.write(0x85);
        serialMgr.waitForOk(500); // wait for deceleration + cancel ok
      }
      while (Serial1.available()) Serial1.read(); // flush any extra oks (jog sends 2 per cmd)
      // Re-unlock so pen commands work cleanly after jog
      Log::send("$X");
      Serial1.println("$X");
      serialMgr.waitForOk(300);
      while (Serial1.available()) Serial1.read();
      Log::nav("JOG: exited");
      _jogging = false; _pendingJog = false; _cancelPending = false; _lastJogSign = 0;
      encoder.setPosition(_selectedControl);
      displayList(encoder.getPosition(), control_menuItems, control_menuSize);
    }
    return;
  }

  if (encoder.turned()) {
    encoder.constrainPosition(0, control_menuSize - 1);
    displayList(encoder.getPosition(), control_menuItems, control_menuSize);
  }
  if (encoder.pressed()) control_menuSelect(encoder.getPosition());
}

void control_menuSelect(int i) {
  _selectedControl = i;
  if      (i == 0) { currentState = MAIN; control_active = false; }
  else if (i == 1) { sendPen(g_penUpS,   true,  "pen up"); }
  else if (i == 2) { sendPen(g_penDownS, false, "pen down"); }
  else if (i == 3 || i == 4) {
    _jogAxis = i - 3; _jogAccum = 0; _jogAlarm = false;
    _pendingJog = false; _cancelPending = false; _lastJogSign = 0;
    _lastJogMs = millis(); _lastDisplayMs = 0; _dispPos = 0;
    encoder.setPosition(0); _jogging = true;
    displayJog(_jogAxis == 0 ? 'X' : 'Y', 0, 0);
  }
}
