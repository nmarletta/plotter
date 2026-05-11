#pragma once
#include <Arduino.h>
#include <SdFat.h>

extern SdFat sd;

enum class GCodeStatus {
    Idle,
    Running,
    Paused,
    WaitingForOk,
    Completed,
    Error,
    Canceled
};

class GCodeStreamer {
public:
    GCodeStreamer(HardwareSerial &grblSerial, uint8_t csPin);
    bool begin(); // initialize SD
    bool start(const char *filepath);
    void pause();
    void resume();
    void cancel(); // stop and clear progress
    void stop(); // stop but keep progress file
    void tick(); // call frequently from loop()
    GCodeStatus status() const;
    uint32_t currentLine() const;
    String currentFilename() const;

private:
    HardwareSerial &_grbl;
    FsFile _file;
    String _filepath;
    uint32_t _lineNumber = 0; // 0-based
    GCodeStatus _status = GCodeStatus::Idle;
    bool _waitingForOk = false;
    unsigned long _lastProgressSaveMs = 0;
    const unsigned long _progressSaveIntervalMs = 3000;
    uint8_t _sdCSPin;

    bool openFileAtLine(const char *path, uint32_t lineToStart);
    bool sendNextLine();
    void handleGrblResponse();
    bool saveProgress();
    bool loadProgress(uint32_t &lineOut);
    String progressPathFor(const String &gcodePath) const;
    String readLineFromFile(FsFile &f);
};