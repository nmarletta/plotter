#include "state_plot.h"

#define PIN_FAN 2

StatePlot::StatePlot(GCodeStreamer &streamer) : _stream(streamer) {}

void StatePlot::enter(const char *filepath) {
    _filepath = String(filepath);
    _menuIndex = 0;
    _confirmCancel = false;
    _showingPaused = false;

    // start streamer (captures file size internally for progress)
    _stream.start(filepath);
    digitalWrite(PIN_FAN, HIGH); // fan on while plotting
    refreshDisplay();
}

void StatePlot::exit() {
    // ensure streamer is stopped or left as desired
    _stream.stop();
    digitalWrite(PIN_FAN, LOW); // fan off when done
}

void StatePlot::loopTick() {
    // drive streamer
    _stream.tick();

    _showingPaused = (_stream.status() == GCodeStatus::Paused);

    // Throttle display to 1Hz — enough for a progress bar on a long plot
    if (millis() - _lastDisplayMs >= 1000) {
        _lastDisplayMs = millis();
        refreshDisplay();
    }
}

void StatePlot::onEncoderRotate(int absIndex) {
    _menuIndex = absIndex;
    refreshDisplay();
}

void StatePlot::onButtonPress() {
    // Alarm state: single Unlock button — send $X, reopen file, go to Paused
    if (_stream.status() == GCodeStatus::Alarm) {
        _stream.unlock();
        refreshDisplay();
        return;
    }

    // Error state: Retry soft-resets GRBL and auto-resumes from saved progress when GRBL ready
    if (_stream.status() == GCodeStatus::Error) {
        if (_menuIndex == 0) {
            _stream.retryAfterError();
        } else {
            doCancelConfirmed();
        }
        refreshDisplay();
        return;
    }

    // act on selected item
    if (_menuIndex == 0) {
        // Pause / Continue
        if (_stream.status() == GCodeStatus::Paused) {
            _stream.resume();
        } else if (_stream.status() == GCodeStatus::Running) {
            _stream.pause();
        } else if (_stream.status() == GCodeStatus::Idle) {
            // no-op or restart
        }
    } else if (_menuIndex == 1) {
        // Cancel -> ask confirm
        if (!_confirmCancel) {
            _confirmCancel = true;
        } else {
            // confirmed
            doCancelConfirmed();
        }
    }
    refreshDisplay();
}

void StatePlot::doCancelConfirmed() {
    _stream.cancel(); // sets status Canceled; free function switches to MAIN on next tick
    _confirmCancel = false;
}

// ---- Free function wrapper for main.cpp state machine ----
// Uses static instances so GCodeStreamer and StatePlot persist across loop calls.

extern RotaryButton encoder;
extern State currentState;

static GCodeStreamer _gcode_streamer(Serial1, A5); // Serial1=GRBL, A5=SD CS pin
static StatePlot     _state_plot(_gcode_streamer);
static bool          _plot_entered = false;

void state_plot() {
  if (!_plot_entered) {
    encoder.setPosition(0);          // reset encoder on entry
    _gcode_streamer.begin();
    _state_plot.enter(selectedFile.c_str());
    _plot_entered = true;
  }

  if (encoder.turned()) {
    // Alarm: single button, constrain to 0. Normal: two buttons 0-1.
    int maxIdx = (_gcode_streamer.status() == GCodeStatus::Alarm) ? 0 : 1;
    encoder.constrainPosition(0, maxIdx);
    _state_plot.onEncoderRotate(encoder.getPosition());
  }

  if (encoder.pressed()) {
    _state_plot.onButtonPress();
  }

  _state_plot.loopTick();

  // Return to MAIN on stream completion or cancellation
  GCodeStatus s = _gcode_streamer.status();
  if (s == GCodeStatus::Completed || s == GCodeStatus::Canceled) {
    _state_plot.exit();
    _plot_entered = false;
    currentState = MAIN;
  }
}

void StatePlot::refreshDisplay() {
    float progress = _stream.progress();
    bool paused = (_stream.status() == GCodeStatus::Paused);

    // Strip directory from filepath for display
    const char* full = _filepath.c_str();
    const char* slash = strrchr(full, '/');
    const char* fname = slash ? slash + 1 : full;

    bool error     = (_stream.status() == GCodeStatus::Error);
    bool resetting = (_stream.status() == GCodeStatus::Resetting);
    int8_t alarm   = _stream.alarmCode();
    displayPlot(fname, _menuIndex, progress, paused, _confirmCancel, error, resetting, alarm);
}

