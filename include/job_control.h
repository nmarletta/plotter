#pragma once
// Lightweight job-control interface used by wifi_manager.
// Avoids pulling in display/U8g2 headers from state_plot.h.
#include "gcode_streamer.h"

GCodeStatus plotStatus();
float        plotProgress();
const char*  plotFilename();
uint32_t     plotCurrentLine();
void         plotPause();
void         plotResume();
void         plotCancel();
bool         plotStartRemote(const char* filepath);
