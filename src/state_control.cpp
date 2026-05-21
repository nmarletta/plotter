#include "state_control.h"
#include "logger.h"
#include "globals.h"

static const int  kJogIntervalMs  = 80;   // ms to accumulate encoder ticks before sending
static const int  kJogFeed        = 1000; // mm/min
static const int  kJogStepMm      = 1;    // mm per encoder tick
static const int  kDisplayMs      = 100;  // ms between jog display updates
static const int  kCancelTimeoutMs = 500; // ms before giving up on cancel ok
static const int  kPenTimeoutMs   = 8000; // ms to wait for ok after pen command

bool control_active = false;

String control_menuItems[] = { "<- Back", "Pen Up", "Pen Down", "Jog X", "Jog Y", "Unlock", "Reset" };
int    control_menuSize    = 7;

static int           _selectedControl = 0;
static bool          _jogging         = false;
static int           _jogAxis         = 0;
static int           _jogAccum        = 0;
static unsigned long _lastJogMs       = 0;
static unsigned long _lastDisplayMs   = 0;
static unsigned long _cancelStartMs   = 0; // when _cancelPending was set
static float         _dispPos         = 0; // local mm position for display
static bool          _jogAlarm        = false;
static bool          _controlAlarm    = false;
static int8_t        _ctrlAlarmCode   = 0;
static int           _alarmBtn        = 0;
static bool          _pendingJog      = false;
static bool          _cancelPending   = false;
static int           _lastJogSign     = 0;

// Flush any bytes sitting in the Serial1 buffer.
static void flushGrbl() {
  delay(50);
  serialMgr.flushRx();
}

// Send a pen command and wait for GRBL's ok.
static void sendPen(int sValue, const char *cmd) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%s S%d", cmd, sValue);
  Log::nav(buf);
  serialMgr.flushRx();
  bool ok = serialMgr.sendLine(buf, kPenTimeoutMs);
  serialMgr.flushRx();
  if (ok) return;

  // Command timed out or errored.
  const char* resp = serialMgr.getLastResponse();
  Log::err(resp);
  if (strcmp(resp, "TIMEOUT") == 0) serialMgr.probeStatus();

  if (strncmp(resp, "ALARM:", 6) == 0) {
    _ctrlAlarmCode = (int8_t)atoi(resp + 6);
  } else if (strcmp(resp, "error:9") == 0) {
    _ctrlAlarmCode = 0;
  } else if (strcmp(resp, "TIMEOUT") == 0) {
    _ctrlAlarmCode = -2;
  } else {
    return;
  }
  _controlAlarm = true;
  _alarmBtn = (_ctrlAlarmCode == -2) ? 1 : 0;
  encoder.setPosition(_alarmBtn);
  displayAlarm(_ctrlAlarmCode, _alarmBtn);
}

// Read pending GRBL bytes; parse ok/alarm.
// No ? polling here — SPI display updates drop UART bytes on SAMD21.
static void pollGrbl() {
  char line[64];
  while (serialMgr.readLine(line, sizeof(line))) {
    if (line[0] != '<') Log::recv(line); // skip status reports
    if (strncmp(line, "ALARM:", 6) == 0) { _jogAlarm = true; _ctrlAlarmCode = (int8_t)atoi(line + 6); }
    if (strncmp(line, "Grbl ",  5) == 0) { _jogAlarm = true; _ctrlAlarmCode = -1; }
    if (strcmp(line, "ok") == 0) { _pendingJog = false; _cancelPending = false; }
  }
}

void state_control() {
  if (!control_active) {
    _jogging = false; _jogAccum = 0;
    encoder.setPosition(0);
    flushGrbl();
    Log::nav("entered control");
    displayList(encoder.getPosition(), control_menuItems, control_menuSize);
    control_active = true;
  }

  if (_jogging) {
    pollGrbl();

    if (_jogAlarm) {
      Log::nav("JOG: alarm");
      Log::sendByte(0x85);
      Serial1.write(0x85);
      while (Serial1.available()) Serial1.read();
      _jogging = false; _jogAlarm = false; _pendingJog = false;
      _cancelPending = false; _lastJogSign = 0;
      _controlAlarm = true; _alarmBtn = 0;
      encoder.setPosition(0);
      displayAlarm(_ctrlAlarmCode, 0);
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

    // exit jogging
    if (encoder.pressed()) {
      if (_pendingJog) {
        // Only send cancel if a jog is actually in-flight; GRBL ignores 0x85 when idle
        Log::sendByte(0x85);
        Serial1.write(0x85);
        serialMgr.waitForOk(500); // wait for deceleration + cancel ok
      }
      while (Serial1.available()) Serial1.read(); // flush any extra oks (jog sends 2 per cmd)
      Log::nav("JOG: exited");
      _jogging = false; _pendingJog = false; _cancelPending = false; _lastJogSign = 0;
      encoder.setPosition(_selectedControl);
      displayList(encoder.getPosition(), control_menuItems, control_menuSize);
    }
    return;
  }

  if (_controlAlarm) {
    if (encoder.turned()) {
      encoder.constrainPosition(0, 1);
      _alarmBtn = encoder.getPosition();
      displayAlarm(_ctrlAlarmCode, _alarmBtn);
    }
    if (encoder.pressed()) {
      if (_alarmBtn == 0) {
        // Unlock: send $X, stay in control
        serialMgr.sendLine("$X", 500);
        serialMgr.flushRx();
        Log::nav("alarm: unlocked");
        _controlAlarm = false; _ctrlAlarmCode = 0; _alarmBtn = 0;
        encoder.setPosition(_selectedControl);
        displayList(encoder.getPosition(), control_menuItems, control_menuSize);
      } else {
        // Reset: soft reset, return to MAIN
        serialMgr.softResetAndWait();
        Log::nav("alarm: soft reset");
        _controlAlarm = false; _ctrlAlarmCode = 0; _alarmBtn = 0;
        control_active = false;
        currentState = MAIN;
        return;
      }
    }
    return;
  }

  // menu selection
  if (encoder.turned()) {
    encoder.constrainPosition(0, control_menuSize - 1);
    displayList(encoder.getPosition(), control_menuItems, control_menuSize);
  }
  if (encoder.pressed()) control_menuSelect(encoder.getPosition());
}

void control_menuSelect(int i) {
  _selectedControl = i;
  if      (i == 0) { currentState = MAIN; control_active = false; }
  else if (i == 1) { sendPen(g_penUpS,   "M4"); }
  else if (i == 2) { sendPen(g_penDownS, "M5"); }
  else if (i == 3 || i == 4) {
    _jogAxis = i - 3; _jogAccum = 0; _jogAlarm = false;
    _pendingJog = false; _cancelPending = false; _lastJogSign = 0;
    _lastJogMs = millis(); _lastDisplayMs = 0; _dispPos = 0;
    encoder.setPosition(0); _jogging = true;
    displayJog(_jogAxis == 0 ? 'X' : 'Y', 0, 0);
  }
  else if (i == 5) {
    bool ok = serialMgr.sendLine("$X", 500);
    serialMgr.flushRx();
    if (ok) Log::nav("unlocked");
    else    Log::err(serialMgr.getLastResponse());
    encoder.setPosition(_selectedControl);
    displayList(encoder.getPosition(), control_menuItems, control_menuSize);
  }
  else if (i == 6) {
    serialMgr.softResetAndWait();
    Log::nav("soft reset");
    control_active = false;
    currentState = MAIN;
  }
}
