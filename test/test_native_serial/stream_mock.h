// StreamMock — controllable Stream for native unit tests.
//
// Pre-load bytes the mock should return from read() via pushLine() / pushBytes().
// After exercising code under test, inspect what was written via tx() / txString().
#pragma once

#include "mocks/Arduino.h"
#include <deque>
#include <vector>
#include <string>

class StreamMock : public Stream {
    std::deque<uint8_t>  _rx;   // bytes available for read()
    std::vector<uint8_t> _tx;   // bytes captured from write()

public:
    // ---- stimulus ----------------------------------------------------------

    void pushBytes(const char* s) {
        while (*s) _rx.push_back((uint8_t)*s++);
    }
    // Push a GRBL-style response line (appends \r\n automatically)
    void pushLine(const char* line) {
        pushBytes(line);
        pushBytes("\r\n");
    }

    // ---- inspection --------------------------------------------------------

    std::string txString() const {
        return std::string(_tx.begin(), _tx.end());
    }
    void clearTx() { _tx.clear(); }
    void clearRx() { _rx.clear(); }
    void clear()   { _rx.clear(); _tx.clear(); }

    // ---- Stream interface --------------------------------------------------

    int available() override { return (int)_rx.size(); }

    int read() override {
        if (_rx.empty()) return -1;
        uint8_t c = _rx.front();
        _rx.pop_front();
        return c;
    }

    size_t write(uint8_t c) override {
        _tx.push_back(c);
        return 1;
    }
};
