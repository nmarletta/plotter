// progress_tracker.h
// ProgressTracker: Persists plot progress to SD card for resume-after-power-loss.
// SD is assumed to be initialized externally (main.cpp setup()).

#pragma once
#include <Arduino.h>
#include <SdFat.h>

extern SdFat sd;

class ProgressTracker {
public:
  // Ensure progress directory exists. Returns true on success.
  bool begin();

  // Save current file + line index to SD. Returns true on success.
  bool saveProgress(const char* file, uint32_t lineIndex);

  // Load saved progress into outFile/lineIndex buffers. Returns true if progress exists.
  bool loadProgress(char* outFile, size_t outSize, uint32_t &lineIndex);

  // Delete any saved progress files. Returns true on success.
  bool clearProgress();

  // Returns true if a progress file exists on SD.
  bool hasProgress();

  // Configure checkpoint cadence (defaults: 50 lines, 5000ms)
  void setCheckpointCadenceLines(uint32_t lines);
  void setCheckpointCadenceMs(uint32_t ms);
};
