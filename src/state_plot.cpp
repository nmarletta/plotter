#include "state_plot.h"

StatePlot::StatePlot(GCodeStreamer &streamer) : _stream(streamer) {}

void StatePlot::enter(const char *filepath) {
    _filepath = String(filepath);
    _menuIndex = 0;
    _confirmCancel = false;
    _showingPaused = false;

    // Count total lines so we can display a progress percentage
    _totalLines = 0;
    FsFile f;
    if (f.open(filepath, O_READ)) {
        while (f.available()) {
            if (f.read() == '\n') _totalLines++;
        }
        f.close();
    }

    // start streamer
    bool ok = _stream.start(filepath);
    if (!ok) {
        // handle error: display message
    }
    refreshDisplay();
}

void StatePlot::exit() {
    // ensure streamer is stopped or left as desired
    _stream.stop();
}

void StatePlot::loopTick() {
    // drive streamer
    _stream.tick();

    // update UI once per loop or at some interval
    // update paused / running flags to change menu
    if (_stream.status() == GCodeStatus::Paused) _showingPaused = true;
    else _showingPaused = false;

    // if completed, automatically return to main state (caller should check)
    if (_stream.status() == GCodeStatus::Completed) {
        // TODO: signal main-state to switch back; here we just show final
        // e.g., set a flag or call a callback (not implemented)
    }

    refreshDisplay();
}

void StatePlot::onEncoderRotate(int delta) {
    // rotate menu selection
    // simple two-item menu: 0 = Pause/Continue, 1 = Cancel
    int itemCount = 2;
    _menuIndex = (_menuIndex + delta + itemCount) % itemCount;
    refreshDisplay();
}

void StatePlot::onButtonPress() {
    // act on selected item
    if (_menuIndex == 0) {
        // Pause / Continue
        if (_stream.status() == GCodeStatus::Paused) {
            _stream.resume();
        } else if (_stream.status() == GCodeStatus::Running || _stream.status() == GCodeStatus::WaitingForOk) {
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
    _stream.cancel();
    // TODO: notify main-state to switch back to main menu/state
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
    _gcode_streamer.begin();
    _state_plot.enter(selectedFile.c_str());
    _plot_entered = true;
  }

  if (encoder.turned()) {
    _state_plot.onEncoderRotate(encoder.getPosition() > 0 ? 1 : -1);
    encoder.setPosition(0);
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
    float progress = (_totalLines > 0)
        ? constrain((float)_stream.currentLine() / (float)_totalLines, 0.0f, 1.0f)
        : 0.0f;
    bool paused = (_stream.status() == GCodeStatus::Paused);

    // Strip directory from filepath for display
    const char* full = _filepath.c_str();
    const char* slash = strrchr(full, '/');
    const char* fname = slash ? slash + 1 : full;

    displayPlot(fname, _menuIndex, progress, paused, _confirmCancel);
}

