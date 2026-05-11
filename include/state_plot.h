#pragma once

#include "globals.h"
#include "display.h"
#include "RotaryButton.h"

#include <Arduino.h>
#include "gcode_streamer.h"

// Lightweight state class for "plot" mode.
// Replace display/encoder calls with your UI libs.

class StatePlot {
public:
    StatePlot(GCodeStreamer &streamer);
    void enter(const char *filepath); // called when entering this state with chosen file
    void exit();
    void loopTick(); // call frequently from main loop
    // Call from your input handlers:
    void onEncoderRotate(int delta); // -1 or +1
    void onButtonPress(); // press event

private:
    GCodeStreamer &_stream;
    String _filepath;
    int _menuIndex = 0;
    bool _confirmCancel = false;
    bool _showingPaused = false;
    uint32_t _totalLines = 0;

    void refreshDisplay();
    void doCancelConfirmed();
};

// Free function called by main.cpp state machine loop.
void state_plot();