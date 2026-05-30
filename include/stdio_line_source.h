// stdio_line_source.h
// LineSource backed by a POSIX FILE* (native PC builds only).
// Fully inline — no separate .cpp needed.
#pragma once
#ifndef ARDUINO

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "line_source.h"

class StdioLineSource : public LineSource {
public:
    StdioLineSource() : _fp(nullptr), _size(0) {}

    ~StdioLineSource() override { close(); }

    bool open(const char* path) override {
        if (_fp) fclose(_fp);
        _fp = fopen(path, "r");
        if (!_fp) { _size = 0; return false; }
        fseek(_fp, 0, SEEK_END);
        _size = (uint32_t)ftell(_fp);
        rewind(_fp);
        return true;
    }

    void close() override {
        if (_fp) { fclose(_fp); _fp = nullptr; }
    }

    bool isOpen() const override { return _fp != nullptr; }

    bool available() override {
        if (!_fp) return false;
        int c = fgetc(_fp);
        if (c == EOF) return false;
        ungetc(c, _fp);
        return true;
    }

    void readLine(char* buf, uint8_t maxLen) override {
        if (!_fp) { if (maxLen > 0) buf[0] = '\0'; return; }
        uint8_t len = 0;
        int c;
        while ((c = fgetc(_fp)) != EOF) {
            if (c == '\r') continue;
            if (c == '\n') break;
            if (len < maxLen - 1) buf[len++] = (char)c;
        }
        buf[len] = '\0';
        // Trim trailing spaces
        while (len > 0 && buf[len - 1] == ' ') buf[--len] = '\0';
        // Trim leading spaces
        uint8_t start = 0;
        while (start < len && buf[start] == ' ') start++;
        if (start > 0) memmove(buf, buf + start, len - start + 1);
    }

    uint32_t fileSize() override { return _size; }

    uint32_t filePos() override {
        if (!_fp) return 0;
        long p = ftell(_fp);
        return p < 0 ? 0 : (uint32_t)p;
    }

private:
    FILE*    _fp;
    uint32_t _size;
};

#endif // !ARDUINO
