// posix_stream.h
// Wraps a POSIX tty file descriptor as an Arduino Stream.
// Used in Phase 2 native tests to talk to real GRBL over USB serial.
#pragma once

#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

// Arduino.h mock is on the include path already.
// We just need Stream, which it provides.
#include <Arduino.h>

class PosixStream : public Stream {
public:
    PosixStream() : _fd(-1), _head(0), _tail(0) {}

    ~PosixStream() { close(); }

    // Open the tty at the given path and configure it for raw 8N1 at baud.
    bool open(const char* port, int baud = 115200) {
        _fd = ::open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (_fd < 0) { perror("PosixStream::open"); return false; }

        struct termios tty;
        memset(&tty, 0, sizeof(tty));
        tcgetattr(_fd, &tty);

        speed_t speed = (baud == 115200) ? B115200 : B9600;
        cfsetispeed(&tty, speed);
        cfsetospeed(&tty, speed);

        tty.c_cflag  =  CS8 | CREAD | CLOCAL;  // 8N1, no flow control
        tty.c_lflag  =  0;                       // raw input
        tty.c_iflag  =  0;                       // no SW flow ctrl, no CR mapping
        tty.c_oflag  =  0;                       // raw output
        tty.c_cc[VMIN]  = 0;                     // non-blocking reads
        tty.c_cc[VTIME] = 1;                     // 100ms read timeout

        tcsetattr(_fd, TCSANOW, &tty);
        tcflush(_fd, TCIOFLUSH);
        return true;
    }

    void close() {
        if (_fd >= 0) { ::close(_fd); _fd = -1; }
    }

    bool isOpen() const { return _fd >= 0; }

    // Stream interface ----------------------------------------------------------

    int available() override {
        _fill();
        return (_tail - _head + kBufSz) % kBufSz;
    }

    int read() override {
        if (available() == 0) return -1;
        uint8_t c = _buf[_head];
        _head = (_head + 1) % kBufSz;
        return c;
    }

    size_t write(uint8_t c) override {
        if (_fd < 0) return 0;
        return (size_t)::write(_fd, &c, 1);
    }

private:
    static const int kBufSz = 256;
    int     _fd;
    uint8_t _buf[kBufSz];
    int     _head, _tail;

    void _fill() {
        if (_fd < 0) return;
        // Keep reading until the tty has no more bytes or the buffer is full.
        while (true) {
            int space = (kBufSz - 1 - (_tail - _head + kBufSz) % kBufSz);
            if (space == 0) break;
            uint8_t c;
            int n = ::read(_fd, &c, 1);
            if (n <= 0) break;
            _buf[_tail] = c;
            _tail = (_tail + 1) % kBufSz;
        }
    }
};
