#include "progress_tracker.h"
#include <Arduino.h>

// File paths used for checkpointing on SD
static const char* kProgressDir = "/.plotter";
static const char* kProgressFile = "/.plotter/progress.json";
static const char* kProgressTmp = "/.plotter/progress.tmp";

// Simple in-memory cadence defaults
static uint32_t cadenceLines = 50;
static uint32_t cadenceMs = 5000;

bool ProgressTracker::begin() {
  if (!sd.exists(kProgressDir)) {
    if (!sd.mkdir(kProgressDir)) {
      return false;
    }
  }
  return true;
}

bool ProgressTracker::saveProgress(const char* file, uint32_t lineIndex) {
  if (!ProgressTracker::begin()) return false;
  FsFile f;
  if (!f.open(kProgressTmp, O_WRONLY | O_CREAT | O_TRUNC)) return false;
  f.print("{\"file\":\"");
  f.print(file);
  f.print("\",\"lineIndex\":");
  f.print(lineIndex);
  f.print("}");
  f.close();
  if (sd.exists(kProgressFile)) sd.remove(kProgressFile);
  FsFile src, dst;
  if (!src.open(kProgressTmp, O_READ)) return false;
  if (!dst.open(kProgressFile, O_WRONLY | O_CREAT | O_TRUNC)) { src.close(); return false; }
  while (src.available()) dst.write(src.read());
  src.close();
  dst.close();
  sd.remove(kProgressTmp);
  return true;
}

bool ProgressTracker::loadProgress(char* outFile, size_t outSize, uint32_t &lineIndex) {
  if (!ProgressTracker::begin()) return false;
  if (!sd.exists(kProgressFile)) return false;
  FsFile f;
  if (!f.open(kProgressFile, O_READ)) return false;
  String contents = f.readString();
  f.close();
  // Very simple parsing (assumes well-formed small JSON)
  int idxFile = contents.indexOf("\"file\":\"");
  if (idxFile < 0) return false;
  idxFile += 9; // move to after "file":"
  int endFile = contents.indexOf('"', idxFile);
  if (endFile < 0) return false;
  String fileStr = contents.substring(idxFile, endFile);
  fileStr.toCharArray(outFile, outSize);
  int idxLine = contents.indexOf("\"lineIndex\":");
  if (idxLine < 0) return false;
  idxLine += 12;
  String lineStr = contents.substring(idxLine);
  lineIndex = (uint32_t)lineStr.toInt();
  return true;
}

bool ProgressTracker::clearProgress() {
  if (!ProgressTracker::begin()) return false;
  if (sd.exists(kProgressTmp)) sd.remove(kProgressTmp);
  if (sd.exists(kProgressFile)) sd.remove(kProgressFile);
  return true;
}

bool ProgressTracker::hasProgress() {
  if (!ProgressTracker::begin()) return false;
  return sd.exists(kProgressFile);
}

void ProgressTracker::setCheckpointCadenceLines(uint32_t lines) { cadenceLines = lines; }
void ProgressTracker::setCheckpointCadenceMs(uint32_t ms) { cadenceMs = ms; }
