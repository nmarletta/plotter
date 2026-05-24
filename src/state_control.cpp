#include "state_control.h"
#include "logger.h"
#include "globals.h"
#include "grbl_errors.h"

static const int  kJogIntervalMs   = 80;    // ms to accumulate encoder ticks before sending
static const int  kJogFeed         = 1000;  // mm/min
static const int  kJogStepMm       = 1;     // mm per encoder tick
static const int  kDisplayMs       = 100;   // ms between jog display updates
static const int  kCancelTimeoutMs = 500;   // ms before giving up on cancel ok
static const int  kPenTimeoutMs    = 8000;  // ms to wait for ok after pen command
static const int  kHomeTimeoutMs   = 30000; // ms to wait for homing cycle

bool control_active = false;

// ── menu ─────────────────────────────────────────────────────────────────────
static const char* const control_menuItems[] = {
  "Back", "Pen Up", "Pen Down", "Jog X", "Jog Y", "Home", "Unlock", "Reset"
};
static const int control_menuSize = 8;

// ── state ─────────────────────────────────────────────────────────────────────
static int           _selectedControl = 0;
static bool          _jogging         = false;
static int           _jogAxis         = 0;   // 0=X 1=Y
static int           _jogAccum        = 0;
static unsigned long _lastJogMs       = 0;
static unsigned long _lastDisplayMs   = 0;
static unsigned long _cancelStartMs   = 0;
static float         _dispX           = 0;
static float         _dispY           = 0;
static float         _jogOriginX      = 0;
static float         _jogOriginY      = 0;
static bool          _needsPosRefresh = false;
static bool          _jogAlarm        = false;
static bool          _controlAlarm    = false;
static int8_t        _ctrlAlarmCode   = 0;
static int           _alarmBtn        = 0;
static bool          _pendingJog      = false;
static bool          _cancelPending   = false;
static int           _lastJogSign     = 0;
static char          _ctrlFeedback[32] = "";
static char          _jogError[32]     = "";
static bool          _screen_dirty    = true;

// ── helpers ───────────────────────────────────────────────────────────────────
static void flushGrbl() {
  delay(50);
  serialMgr.flushRx();
}

static void renderControl() {
  displayControl(
    _selectedControl, control_menuSize,
    control_menuItems[_selectedControl],
    _ctrlFeedback
  );
}

static void sendPen(int sValue, const char *cmd) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%s S%d", cmd, sValue);
  Log::nav(buf);
  serialMgr.flushRx();
  bool ok = serialMgr.sendLine(buf, kPenTimeoutMs);
  serialMgr.flushRx();
  if (ok) {
    snprintf(_ctrlFeedback, sizeof(_ctrlFeedback), "ok");
    return;
  }

  const char* resp = serialMgr.getLastResponse();
  snprintf(_ctrlFeedback, sizeof(_ctrlFeedback), "%s", grblResponseStr(resp));
  Log::err(resp);

  if (strncmp(resp, "ALARM:", 6) == 0) {
    _ctrlAlarmCode = (int8_t)atoi(resp + 6);
  } else if (strcmp(resp, "error:9") == 0) {
    _ctrlAlarmCode = 0;
  } else if (strcmp(resp, "TIMEOUT") == 0) {
    _ctrlAlarmCode = -2;
    serialMgr.probeStatus();
  } else {
    return;
  }
  _controlAlarm = true;
  _alarmBtn = (_ctrlAlarmCode == -2) ? 1 : 0;
  encoder.setPosition(_alarmBtn);
  displayAlarm(_ctrlAlarmCode, _alarmBtn);
}

static bool parseGrblPos(const char* line, float& x, float& y) {
  const char* pos = strstr(line, "MPos:");
  if (!pos) pos = strstr(line, "WPos:");
  if (!pos) return false;
  pos += 5;
  x = atof(pos);
  const char* comma = strchr(pos, ',');
  if (!comma) return false;
  y = atof(comma + 1);
  return true;
}

static bool queryGrblPos(float& x, float& y) {
  serialMgr.flushRx();
  Serial1.print('?');
  unsigned long t0 = millis();
  char line[64];
  while (millis() - t0 < 300) {
    if (serialMgr.readLine(line, sizeof(line)) && line[0] == '<') {
      return parseGrblPos(line, x, y);
    }
  }
  return false;
}

static void pollGrbl() {
  char line[64];
  while (serialMgr.readLine(line, sizeof(line))) {
    if (line[0] == '<') {
      if (_needsPosRefresh) {
        float mx, my;
        if (parseGrblPos(line, mx, my)) {
          _dispX = mx - _jogOriginX;
          _dispY = my - _jogOriginY;
          _jogError[0] = '\0';
          _needsPosRefresh = false;
        }
      }
      continue;
    }
    Log::recv(line);
    if (strncmp(line, "ALARM:", 6) == 0) { _jogAlarm = true; _ctrlAlarmCode = (int8_t)atoi(line + 6); }
    if (strncmp(line, "Grbl ",  5) == 0) { _jogAlarm = true; _ctrlAlarmCode = -1; }
    if (strcmp(line, "ok") == 0)         { _pendingJog = false; _cancelPending = false; }
    if (strncmp(line, "error:", 6) == 0) {
      _pendingJog = false; _cancelPending = false;
      _jogAccum = 0;
      _needsPosRefresh = true;
      Serial1.print('?');
      snprintf(_jogError, sizeof(_jogError), "%s", grblErrorStr(atoi(line + 6)));
      Log::err(line);
    }
  }
}

// ── main loop ─────────────────────────────────────────────────────────────────
void state_control() {
  if (!control_active) {
    _jogging = false; _jogAccum = 0;
    _dispX = 0; _dispY = 0;
    _selectedControl = 0;
    _ctrlFeedback[0] = '\0';
    _screen_dirty = true;
    encoder.setPosition(0);
    flushGrbl();
    Log::nav("entered control");
    control_active = true;
  }

  // ── jogging ──────────────────────────────────────────────────────────────
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
      if (_jogAxis == 0) _dispX += delta;
      else               _dispY += delta;
      _jogError[0] = '\0';
      encoder.setPosition(0);
    }

    if (millis() - _lastDisplayMs >= kDisplayMs) {
      _lastDisplayMs = millis();
      float pos = (_jogAxis == 0) ? _dispX : _dispY;
      displayJog(_jogAxis == 0 ? 'X' : 'Y', pos, _jogError);
    }

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
        Log::sendByte(0x85);
        Serial1.write(0x85);
        serialMgr.waitForOk(500);
      }
      while (Serial1.available()) Serial1.read();
      Log::nav("JOG: exited");

      float finalPos = (_jogAxis == 0) ? _dispX : _dispY;
      snprintf(_ctrlFeedback, sizeof(_ctrlFeedback), "%+.1f mm", (double)finalPos);

      _jogging = false; _pendingJog = false; _cancelPending = false; _lastJogSign = 0;
      encoder.setPosition(_selectedControl);
      _screen_dirty = true;
    }
    return;
  }

  // ── alarm sub-state ───────────────────────────────────────────────────────
  if (_controlAlarm) {
    if (encoder.turned()) {
      encoder.constrainPosition(0, 1);
      _alarmBtn = encoder.getPosition();
      displayAlarm(_ctrlAlarmCode, _alarmBtn);
    }
    if (encoder.pressed()) {
      if (_alarmBtn == 0) {
        serialMgr.sendLine("$X", 500);
        serialMgr.flushRx();
        Log::nav("alarm: unlocked");
        snprintf(_ctrlFeedback, sizeof(_ctrlFeedback), "ok");
        _controlAlarm = false; _ctrlAlarmCode = 0; _alarmBtn = 0;
        encoder.setPosition(_selectedControl);
        _screen_dirty = true;
      } else {
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

  // ── normal navigation ─────────────────────────────────────────────────────
  if (_screen_dirty) {
    _screen_dirty = false;
    renderControl();
  }

  if (encoder.turned()) {
    encoder.constrainPosition(0, control_menuSize - 1);
    _selectedControl = encoder.getPosition();
    _ctrlFeedback[0] = '\0';
    _screen_dirty = true;
  }

  if (encoder.pressed()) {
    control_menuSelect(encoder.getPosition());
    _screen_dirty = true;
  }
}

// ── menu select ───────────────────────────────────────────────────────────────
void control_menuSelect(int i) {
  _selectedControl = i;

  if (i == 0) {
    currentState = MAIN;
    control_active = false;

  } else if (i == 1) {
    sendPen(g_penUpS, "M4");

  } else if (i == 2) {
    sendPen(g_penDownS, "M5");

  } else if (i == 3 || i == 4) {
    _jogAxis = i - 3;
    _jogAccum = 0; _dispX = 0; _dispY = 0;
    _jogOriginX = 0; _jogOriginY = 0;
    _jogAlarm = false; _pendingJog = false;
    _cancelPending = false; _lastJogSign = 0;
    _jogError[0] = '\0'; _needsPosRefresh = false;
    _lastJogMs = millis(); _lastDisplayMs = 0;
    queryGrblPos(_jogOriginX, _jogOriginY);
    encoder.setPosition(0);
    _jogging = true;
    displayJog(_jogAxis == 0 ? 'X' : 'Y', 0.0f, nullptr);

  } else if (i == 5) {
    snprintf(_ctrlFeedback, sizeof(_ctrlFeedback), "Homing...");
    renderControl();
    bool ok = serialMgr.sendLine("$H", kHomeTimeoutMs);
    serialMgr.flushRx();
    if (ok) {
      snprintf(_ctrlFeedback, sizeof(_ctrlFeedback), "ok");
      Log::nav("homed");
    } else {
      const char* resp = serialMgr.getLastResponse();
      snprintf(_ctrlFeedback, sizeof(_ctrlFeedback), "%s", grblResponseStr(resp));
      Log::err(resp);
      if (strncmp(resp, "ALARM:", 6) == 0) {
        _ctrlAlarmCode = (int8_t)atoi(resp + 6);
        _controlAlarm = true; _alarmBtn = 0;
        encoder.setPosition(0);
        displayAlarm(_ctrlAlarmCode, 0);
        return;
      }
    }
    encoder.setPosition(_selectedControl);

  } else if (i == 6) {
    bool ok = serialMgr.sendLine("$X", 500);
    serialMgr.flushRx();
    if (ok) {
      snprintf(_ctrlFeedback, sizeof(_ctrlFeedback), "ok");
      Log::nav("unlocked");
    } else {
      const char* unlockResp = serialMgr.getLastResponse();
      snprintf(_ctrlFeedback, sizeof(_ctrlFeedback), "%s", grblResponseStr(unlockResp));
      Log::err(unlockResp);
    }
    encoder.setPosition(_selectedControl);

  } else if (i == 7) {
    serialMgr.softResetAndWait();
    Log::nav("soft reset");
    control_active = false;
    currentState = MAIN;
  }
}
