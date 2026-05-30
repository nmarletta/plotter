// sd_line_source.h
// LineSource backed by SdFat FsFile (Arduino builds only).
// Fully inline — no separate .cpp needed.
#pragma once
#ifdef ARDUINO

#include <SdFat.h>
#include <string.h>
#include "line_source.h"

extern SdFat sd;

class SdLineSource : public LineSource {
public:
    bool open(const char* path) override {
        if (_file) _file.close();
        return _file.open(path, O_READ);
    }

    void close() override {
        if (_file) _file.close();
    }

    bool isOpen() const override { return (bool)_file; }

    bool available() override { return _file.available(); }

    void readLine(char* buf, uint8_t maxLen) override {
        uint8_t len = 0;
        while (_file.available()) {
            char c = (char)_file.read();
            if (c == '\r') continue;
            if (c == '\n') break;
            if (len < maxLen - 1) buf[len++] = c;
        }
        buf[len] = '\0';
        // Trim trailing spaces
        while (len > 0 && buf[len - 1] == ' ') buf[--len] = '\0';
        // Trim leading spaces
        uint8_t start = 0;
        while (start < len && buf[start] == ' ') start++;
        if (start > 0) memmove(buf, buf + start, len - start + 1);
    }

    uint32_t fileSize() override { return (uint32_t)_file.size(); }
    uint32_t filePos()  override { return (uint32_t)_file.curPosition(); }

private:
    FsFile _file;
};

#endif // ARDUINO
