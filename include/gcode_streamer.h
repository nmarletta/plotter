#pragma once
#include <Arduino.h>
#include "line_source.h"

// SdFat and the global 'sd' are only needed in Arduino builds.
#ifdef ARDUINO
#include <SdFat.h>
extern SdFat sd;
#endif

enum class GCodeStatus {
    Idle,
    Running,    // actively sending (may have lines in-flight)
    Paused,
    Completed,
    Error,
    Canceled,
    Resetting,  // soft reset sent, waiting for GRBL banner before resuming
    Alarm       // GRBL alarm triggered (e.g. hard/soft limit hit)
};

class GCodeStreamer {
public:
    // Inject the GRBL serial stream and the G-code file source.
    // On Arduino: pass Serial1 (HardwareSerial extends Stream) + SdLineSource.
    // On PC:      pass PosixStream + StdioLineSource.
    GCodeStreamer(Stream& grbl, LineSource& src);

    bool begin();
    bool start(const char* filepath);
    void pause();
    void resume();
    void cancel();
    void retryAfterError();
    void stop();
    void tick();

    GCodeStatus  status()          const;
    uint32_t     currentLine()     const;
    float        progress();               // non-const: reads file position
    int8_t       alarmCode()       const;
    const char*  currentFilename() const;
    void         unlock();

    // Dry-run: apply all filters and print each line that would be sent to `out`.
    // Does not write to the GRBL stream. Requires the LineSource to be openable.
    void trace(const char* filepath, Print& out);

private:
    Stream&     _grbl;
    LineSource& _src;

    char     _filepath[64];
    uint32_t _lineNumber = 0;
    GCodeStatus _status  = GCodeStatus::Idle;

    // Character-counting: track bytes in GRBL's 128-byte serial RX buffer
    static const uint8_t  kGrblBufSize    = 127;
    static const uint8_t  kQueueDepth     = 8;
    static const uint32_t kSaveIntervalMs = 5000;
    static const uint32_t kResetTimeoutMs = 3000;

    uint8_t  _pending[kQueueDepth];
    uint8_t  _qHead = 0, _qTail = 0, _qCount = 0;
    uint8_t  _bufUsed = 0;

    char  _pendingLine[80];
    bool  _hasPendingLine = false;
    bool  _sentFinalM5    = false;
    int8_t _alarmCode     = 0;

    unsigned long _lastSaveMs   = 0;
    unsigned long _resetStartMs = 0;

    void pumpLines();
    void handleGrblResponse();
    void processGrblLine(const char* line);
    bool saveProgress();
    bool loadProgress(uint32_t& lineOut);
};
