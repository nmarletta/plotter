#include "state_control.h"

static const int  kJogIntervalMs  = 80;   // accumulate ticks for this long before sending
static const int  kJogFeed        = 1000; // mm/min
static const int  kJogStepMm      = 1;    // mm per encoder tick
static const int  kPosIntervalMs  = 200;  // how often to poll GRBL position

// Servo S values (0–$30, default $30=1000).
// Tune these until pen moves cleanly: start with 1000/0 to confirm servo responds,
// then narrow down to the actual up/down positions.
static const int  kPenDownS       = 700;  // M3 S<n>  — pen touching paper
static const int  kPenUpS         = 300;  // M3 S<n>  — pen lifted

bool control_active = false;

String control_menuItems[] = { "<- Back", "Pen Up", "Pen Down", "Jog X", "Jog Y" };
int    control_menuSize    = 5;

static int           _selectedControl = 0;
static bool          _jogging         = false;
static int           _jogAxis         = 0; // 0=X, 1=Y
static int           _jogAccum        = 0;
static unsigned long _lastJogMs       = 0;
static unsigned long _lastPosMs       = 0;
static float         _posX            = 0;
static float         _posY            = 0;
static char          _grblLine[64];
static uint8_t       _grblLineLen     = 0;

static bool _jogAlarm   = false;
static bool _pendingJog = false; // true while a $J= command is in-flight (no ok yet)
static int  _lastJogSign = 0;   // +1 or -1, to detect direction reversal

// Read any pending GRBL bytes into _grblLine; parse MPos when a full line arrives.
static void pollGrblPosition() {
    while (Serial1.available()) {
        char c = (char)Serial1.read();
        if (c == '\r') continue;
        if (c == '\n') {
            _grblLine[_grblLineLen] = '\0';
            if (_grblLineLen > 0) {
                Serial.print("<< "); Serial.println(_grblLine);
                if (strncmp(_grblLine, "ALARM:", 6) == 0) _jogAlarm = true;
                // ok = previous jog accepted, ready to send the next one
                if (strcmp(_grblLine, "ok") == 0) _pendingJog = false;
            }
            // Parse <...|MPos:x,y,z|...>
            char *mp = strstr(_grblLine, "MPos:");
            if (mp) {
                mp += 5;
                _posX = atof(mp);
                char *comma = strchr(mp, ',');
                if (comma) _posY = atof(comma + 1);
            }
            _grblLineLen = 0;
        } else if (_grblLineLen < sizeof(_grblLine) - 1) {
            _grblLine[_grblLineLen++] = c;
        }
    }
}

void state_control() {
  if (!control_active) {
    _jogging = false;
    _jogAccum = 0;
    encoder.setPosition(0);
    // Unlock GRBL before any manual commands ($X is safe even if no alarm)
    Serial1.println("$X");
    serialMgr.waitForOk(300); // consume the "ok" (or error, if already Idle)
    Serial.println("CTRL: entered, sent $X");
    displayList(encoder.getPosition(), control_menuItems, control_menuSize);
    control_active = true;
  }

  if (_jogging) {
    // Drain GRBL responses and parse position
    pollGrblPosition();

    // Exit jog if GRBL raised an alarm (hard limit, etc.)
    if (_jogAlarm) {
      Serial.println("JOG: ALARM detected, exiting jog");
      Serial1.write(0x85);
      while (Serial1.available()) Serial1.read();
      _jogging     = false;
      _jogAlarm    = false;
      _pendingJog  = false;
      _lastJogSign = 0;
      encoder.setPosition(_selectedControl);
      displayList(encoder.getPosition(), control_menuItems, control_menuSize);
      return;
    }

    // Accumulate encoder ticks (1 tick = 1mm)
    if (encoder.turned()) {
      _jogAccum += encoder.getPosition() * kJogStepMm;
      encoder.setPosition(0);
    }

    // Periodically request position from GRBL
    if (millis() - _lastPosMs >= kPosIntervalMs) {
      Serial1.write('?');
      _lastPosMs = millis();
      displayJog(_jogAxis == 0 ? 'X' : 'Y', _posX, _posY);
    }

    // Flush accumulated movement — acknowledgment-paced:
    // send next jog only after previous one is acknowledged to avoid flooding GRBL's planner.
    if (millis() - _lastJogMs >= kJogIntervalMs && _jogAccum != 0) {
      int newSign = (_jogAccum > 0) ? 1 : -1;
      // Direction reversed while a jog is in-flight: cancel it first.
      if (_pendingJog && newSign != _lastJogSign) {
        Serial1.write(0x85);
        _pendingJog = false;
        Serial.println("JOG: cancel (direction change)");
      }
      if (!_pendingJog) {
        char buf[40];
        snprintf(buf, sizeof(buf), "$J=G21G91%c%dF%d",
                 _jogAxis == 0 ? 'X' : 'Y', _jogAccum, kJogFeed);
        Serial.print("JOG >> "); Serial.println(buf);
        Serial1.println(buf);
        _pendingJog  = true;
        _lastJogSign = newSign;
        _jogAccum    = 0;
        _lastJogMs   = millis();
      }
    }

    // Button press: exit jog mode
    if (encoder.pressed()) {
      Serial1.write(0x85); // cancel any in-flight jog
      while (Serial1.available()) Serial1.read();
      _jogging     = false;
      _pendingJog  = false;
      _lastJogSign = 0;
      encoder.setPosition(_selectedControl);
      displayList(encoder.getPosition(), control_menuItems, control_menuSize);
    }
    return;
  }

  // Menu mode
  if (encoder.turned()) {
    encoder.constrainPosition(0, control_menuSize - 1);
    displayList(encoder.getPosition(), control_menuItems, control_menuSize);
  }

  if (encoder.pressed()) {
    control_menuSelect(encoder.getPosition());
  }
}

void control_menuSelect(int i) {
  _selectedControl = i;
  if (i == 0) {
    currentState = MAIN;
    control_active = false;
  } else if (i == 1) {
    Serial.println("CTRL: pen up");
    char buf[16]; snprintf(buf, sizeof(buf), "M3 S%d", kPenUpS);
    Serial1.println(buf);
    bool ok = serialMgr.waitForOk(500);
    Serial.print("PEN << "); Serial.println(ok ? "ok" : serialMgr.getLastResponse());
  } else if (i == 2) {
    Serial.println("CTRL: pen down");
    char buf[16]; snprintf(buf, sizeof(buf), "M3 S%d", kPenDownS);
    Serial1.println(buf);
    bool ok = serialMgr.waitForOk(500);
    Serial.print("PEN << "); Serial.println(ok ? "ok" : serialMgr.getLastResponse());
  } else if (i == 3 || i == 4) {
    _jogAxis     = i - 3; // 3=X(0), 4=Y(1)
    _jogAccum    = 0;
    _jogAlarm    = false;
    _pendingJog  = false;
    _lastJogSign = 0;
    _lastJogMs   = millis();
    _lastPosMs = 0; // force immediate position poll on first tick
    encoder.setPosition(0);
    _jogging = true;
    displayJog(_jogAxis == 0 ? 'X' : 'Y', _posX, _posY);
  }
}