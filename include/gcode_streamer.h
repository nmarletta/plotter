#pragma once
#include <Arduino.h>
#include <SdFat.h>

extern SdFat sd;

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
    GCodeStreamer(HardwareSerial &grblSerial, uint8_t csPin);
    bool begin(); // initialize SD
    bool start(const char *filepath);
    void pause();
    void resume();
    void cancel();          // soft reset GRBL, clear progress file, return to Canceled
    void retryAfterError(); // soft reset GRBL, keep progress file, auto-resume when GRBL ready
    void stop();
    void tick(); // call frequently from loop()
    GCodeStatus status() const;
    uint32_t currentLine() const;
    float progress() const;   // 0.0–1.0 based on file byte position
    int8_t alarmCode() const; // non-zero when status == Alarm
    String currentFilename() const;
    void unlock();            // send $X, reopen file at current line, go to Paused

private:
    HardwareSerial &_grbl;
    FsFile _file;
    String _filepath;
    uint32_t _lineNumber = 0;
    GCodeStatus _status = GCodeStatus::Idle;
    uint8_t _sdCSPin;

    // Non-blocking serial line accumulator
    char    _rxBuf[96];
    uint8_t _rxLen = 0;

    // Character-counting: track bytes in GRBL's 128-byte serial RX buffer
    static const uint8_t kGrblBufSize = 127;
    static const uint8_t kQueueDepth  = 8;
    uint8_t _pending[kQueueDepth]; // byte lengths of sent-but-unacked lines
    uint8_t _qHead = 0, _qTail = 0, _qCount = 0;
    uint8_t _bufUsed = 0;

    // Line read from file but GRBL buffer was full — retry next tick
    String   _pendingLine;
    bool     _hasPendingLine = false;
    bool     _sentFinalM5   = false;
    int8_t   _alarmCode     = 0;
    uint32_t _fileSize = 0;

    // Periodic progress save (5s)
    unsigned long _lastSaveMs = 0;
    static const unsigned long kSaveIntervalMs = 5000;

    // Resetting timeout (3s)
    unsigned long _resetStartMs = 0;
    static const unsigned long kResetTimeoutMs = 3000;

    bool openFileAtLine(const char *path, uint32_t lineToStart);
    void pumpLines();
    void handleGrblResponse();
    void processGrblLine(const char *line);
    bool saveProgress();
    bool loadProgress(uint32_t &lineOut);
    String progressPathFor(const String &gcodePath) const;
    String readLineFromFile(FsFile &f);
};