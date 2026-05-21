// Minimal Arduino stubs for native (host PC) builds.
// Replaces <Arduino.h> when compiling tests with platform = native.
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <string>
#include <chrono>
#include <unistd.h>

// ---- Timing ----------------------------------------------------------------

inline unsigned long millis() {
    using namespace std::chrono;
    // static local is deduplicated across TUs by the linker (COMDAT)
    static auto epoch = steady_clock::now();
    return (unsigned long)duration_cast<milliseconds>(steady_clock::now() - epoch).count();
}
inline void delay(unsigned long ms) { usleep((useconds_t)(ms * 1000)); }

// ---- Character helpers -----------------------------------------------------

inline bool isDigit(int c) { return isdigit((unsigned char)c) != 0; }
// toupper() from <ctype.h> is already correct

// ---- Print / Stream --------------------------------------------------------

class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t c) = 0;
    size_t write(const char* s) {
        size_t n = 0;
        while (*s) n += write((uint8_t)*s++);
        return n;
    }
    size_t print(const char* s)  { return write(s); }
    size_t print(char c)         { return write((uint8_t)c); }
    size_t println(const char* s){ size_t n = write(s); return n + write("\r\n"); }
    size_t println(char c)       { size_t n = write((uint8_t)c); return n + write("\r\n"); }
    size_t println()             { return write("\r\n"); }
    size_t print(unsigned long v, int base = 10) {
        char buf[24];
        if (base == 16) snprintf(buf, sizeof(buf), "%lX", v);
        else            snprintf(buf, sizeof(buf), "%lu", v);
        return write(buf);
    }
    size_t print(uint8_t v, int base = 10) { return print((unsigned long)v, base); }
    size_t print(int v, int base = 10)     { return print((unsigned long)(long)v, base); }
};

class Stream : public Print {
public:
    virtual int available() = 0;
    virtual int read()      = 0;
    explicit operator bool() const { return true; }
};

// ---- String ----------------------------------------------------------------
// Minimal subset used by SerialManager::querySettings.

class String {
    std::string _s;
public:
    String() {}
    explicit String(const char* s) : _s(s ? s : "") {}
    explicit String(char c) : _s(1, c) {}
    const char* c_str()    const { return _s.c_str(); }
    size_t      length()   const { return _s.size(); }
    bool startsWith(const char* p) const { return _s.find(p) == 0; }
    int  indexOf(char c)   const {
        auto pos = _s.find(c);
        return pos == std::string::npos ? -1 : (int)pos;
    }
    String substring(int from, int to = -1) const {
        return String(to < 0 ? _s.substr(from).c_str()
                             : _s.substr(from, to - from).c_str());
    }
    float toFloat() const { return (float)atof(_s.c_str()); }
    bool operator==(const char* s)   const { return _s == s; }
    bool operator==(const String& o) const { return _s == o._s; }
};

// ---- constrain -------------------------------------------------------------

template<class T>
inline T constrain(T x, T lo, T hi) { return x < lo ? lo : (x > hi ? hi : x); }
