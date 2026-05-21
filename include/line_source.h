// line_source.h
// Abstract G-code file source used by GCodeStreamer.
// No Arduino or SD dependency — implemented by SdLineSource (Arduino)
// and StdioLineSource (native PC tests).
#pragma once
#include <stdint.h>

class LineSource {
public:
    // Open (or reopen) the file from the beginning. Returns false on failure.
    virtual bool open(const char* path) = 0;

    // Close. Safe to call when already closed.
    virtual void close() = 0;

    // True if the file is currently open.
    virtual bool isOpen() const = 0;

    // True if there are more bytes to read.
    virtual bool available() = 0;

    // Read one line: strips \r, stops at \n, null-terminates.
    // Also trims leading/trailing spaces (matches previous readLineFromFile behaviour).
    virtual void readLine(char* buf, uint8_t maxLen) = 0;

    // Skip n lines (default implementation uses readLine).
    virtual void skipLines(uint32_t n) {
        char dummy[80];
        for (uint32_t i = 0; i < n && available(); ++i)
            readLine(dummy, sizeof(dummy));
    }

    // File size in bytes (0 if unknown or not open).
    virtual uint32_t fileSize() { return 0; }

    // Current byte position in the file (0 if unknown or not open).
    virtual uint32_t filePos() { return 0; }

    virtual ~LineSource() = default;
};
