#include "gcode_streamer.h"
#include "gcode_filters.h"
#include "logger.h"
#include "globals.h"

#ifdef ARDUINO
#include <SdFat.h>   // FsFile for saveProgress / loadProgress
extern SdFat sd;
#endif

// ---- Constructor -----------------------------------------------------------

GCodeStreamer::GCodeStreamer(Stream& grbl, LineSource& src)
    : _grbl(grbl), _src(src) {}

bool GCodeStreamer::begin() {
    _status = GCodeStatus::Idle;
    return true;
}

bool GCodeStreamer::start(const char* filepath) {
    if (_status == GCodeStatus::Running) return false;

    strncpy(_filepath, filepath, sizeof(_filepath) - 1);
    _filepath[sizeof(_filepath) - 1] = '\0';
    _qHead = 0; _qTail = 0; _qCount = 0; _bufUsed = 0;
    _hasPendingLine    = false;
    _pendingIsInjected = false;
    _sentFinalM5       = false;
    _completePending   = false;

    serialMgr.sendLine("$X", 500);

    uint32_t resumeLine = 0;
    _lineNumber = loadProgress(resumeLine) ? resumeLine : 0;

    if (!_src.open(filepath)) {
        char msg[80]; snprintf(msg, sizeof(msg), "cannot open: %s", filepath);
        Log::err(msg);
        _status = GCodeStatus::Error;
        return false;
    }

    // When resuming mid-file, re-send preamble setup lines (before first XY motion).
    if (_lineNumber > 0) {
        char msg[80]; snprintf(msg, sizeof(msg), "resume: %s from line %lu", filepath, (unsigned long)_lineNumber);
        Log::str(msg);
        char pLine[80], up[80];
        while (_src.available()) {
            _src.readLine(pLine, sizeof(pLine));
            if (!filterLine(pLine)) continue;
            // Skip system commands ($H, $X, etc.) — not setup lines.
            // Sending $H here would start a 30-second homing cycle but only
            // wait 500 ms for the ok.  The late ok would then be mis-counted
            // as an ack for the first streaming command, corrupting the queue.
            if (pLine[0] == '$') continue;
            applyPenOverwrite(pLine, sizeof(pLine));
            strncpy(up, pLine, sizeof(up) - 1); up[sizeof(up) - 1] = '\0';
            for (int i = 0; up[i]; i++) up[i] = (char)toupper((unsigned char)up[i]);
            if (strchr(up, 'X') || strchr(up, 'Y')) break;
            serialMgr.sendLine(pLine, 500);
        }
        // Reopen and advance to the resume line.
        if (!_src.open(filepath)) { _status = GCodeStatus::Error; return false; }
        _src.skipLines(_lineNumber);
    }

    _status     = GCodeStatus::Running;
    _lastSaveMs = millis();
    if (_lineNumber == 0) {
        char msg[80]; snprintf(msg, sizeof(msg), "start: %s", filepath);
        Log::str(msg);
    }
    return true;
}

void GCodeStreamer::pause() {
    if (_status == GCodeStatus::Running) {
        _grbl.write('!');
        _status = GCodeStatus::Paused;
        char msg[40]; snprintf(msg, sizeof(msg), "paused at line %lu", (unsigned long)_lineNumber);
        Log::str(msg);
    }
}

void GCodeStreamer::resume() {
    if (_status == GCodeStatus::Paused) {
        _grbl.write('~');
        _status = GCodeStatus::Running;
        char msg[40]; snprintf(msg, sizeof(msg), "resumed at line %lu", (unsigned long)_lineNumber);
        Log::str(msg);
        pumpLines();
    }
}

void GCodeStreamer::cancel() {
    _grbl.write(0x18);
    _status = GCodeStatus::Canceled;
    char msg[40]; snprintf(msg, sizeof(msg), "canceled at line %lu", (unsigned long)_lineNumber);
    Log::str(msg);
    _qHead = 0; _qTail = 0; _qCount = 0; _bufUsed = 0;
    _hasPendingLine    = false;
    _pendingIsInjected = false;
#ifdef ARDUINO
    char p[80]; progressPathFor(_filepath, p, sizeof(p));
    if (sd.exists(p)) sd.remove(p);
#endif
    _src.close();
}

void GCodeStreamer::retryAfterError() {
    _grbl.write(0x18);
    _status = GCodeStatus::Resetting;
    _resetStartMs = millis();
    char msg[64]; snprintf(msg, sizeof(msg), "retry: soft reset, will resume from line %lu", (unsigned long)_lineNumber);
    Log::str(msg);
    _qHead = 0; _qTail = 0; _qCount = 0; _bufUsed = 0;
    _hasPendingLine    = false;
    _pendingIsInjected = false;
    _src.close();
    // Progress file intentionally kept — start() will resume from saved line.
}

void GCodeStreamer::stop() {
    _src.close();
    _status = GCodeStatus::Idle;
}

GCodeStatus  GCodeStreamer::status()          const { return _status; }
uint32_t     GCodeStreamer::currentLine()     const { return _lineNumber; }
int8_t       GCodeStreamer::alarmCode()       const { return _alarmCode; }
const char*  GCodeStreamer::currentFilename() const { return _filepath; }

float GCodeStreamer::progress() {
    if (_status == GCodeStatus::Completed) return 1.0f;
    uint32_t sz = _src.fileSize();
    if (sz == 0) return 0.0f;
    float p = (float)_src.filePos() / (float)sz;
    return p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
}

void GCodeStreamer::unlock() {
    serialMgr.sendLine("$X", 500);
    _qHead = 0; _qTail = 0; _qCount = 0; _bufUsed = 0;
    _hasPendingLine    = false;
    _pendingIsInjected = false;
    _alarmCode = 0;
    _src.open(_filepath);
    _src.skipLines(_lineNumber);
    _status = GCodeStatus::Paused;
}

void GCodeStreamer::tick() {
    if (_status == GCodeStatus::Idle || _status == GCodeStatus::Completed || _status == GCodeStatus::Error) return;
    handleGrblResponse();

    if (_status == GCodeStatus::Paused || _status == GCodeStatus::Canceled || _status == GCodeStatus::Alarm) return;
    if (_status == GCodeStatus::Completed) return;  // completePlot() may have been called above

    if (_status == GCodeStatus::Resetting) {
        if (millis() - _resetStartMs > kResetTimeoutMs) {
            Log::err("STR: reset timeout — no Grbl banner received");
            _status = GCodeStatus::Error;
        }
        return;
    }

    // Don't save progress once we're waiting for idle — the job is effectively done.
    if (!_completePending) {
        if (millis() - _lastSaveMs > kSaveIntervalMs) {
            saveProgress();
            _lastSaveMs = millis();
        }
    }
    pumpLines();

    // Idle-wait: poll '?' every 250 ms; complete when GRBL reports <Idle|…>.
    // Falls back after kIdleWaitTimeoutMs in case the response is dropped (SPI/UART).
    if (_completePending) {
        unsigned long now = millis();
        if (now - _idlePollMs >= 250) {
            _grbl.print('?');
            _idlePollMs = now;
        }
        if (now - _completePendingMs >= kIdleWaitTimeoutMs) {
            Log::err("STR: idle wait timeout, completing anyway");
            _completePending = false;
            completePlot();
        }
    }
}

void GCodeStreamer::pumpLines() {
    char line[80];
    while (_status == GCodeStatus::Running) {
        if (_hasPendingLine) {
            strncpy(line, _pendingLine, sizeof(line) - 1);
            line[sizeof(line) - 1] = '\0';
            _hasPendingLine = false;
        } else if (_src.isOpen() && _src.available()) {
            _src.readLine(line, sizeof(line));
            if (!filterLine(line)) { _lineNumber++; continue; }
            injectMissingS(line, sizeof(line));
            applyPenOverwrite(line, sizeof(line));
        } else if (!_sentFinalM5) {
            // Sanity-check: if the file position is way less than the file size,
            // available() returned false early — likely an SD read error.
            // Log it so the serial monitor shows what happened.
            {
                uint32_t fPos = _src.filePos();
                uint32_t fSz  = _src.fileSize();
                if (fSz > 512 && fPos < fSz / 8) {
                    char warn[72];
                    snprintf(warn, sizeof(warn),
                             "STR: EOF at %lu/%lu bytes — SD error or short read?",
                             (unsigned long)fPos, (unsigned long)fSz);
                    Log::err(warn);
                }
            }
            if (g_overwriteS) snprintf(line, sizeof(line), "M4 S%d", g_penUpS);
            else              strncpy(line, "M4", sizeof(line));
            _sentFinalM5 = true;
        } else {
            // All file lines + final pen-up acked.
            // Don't complete yet — GRBL may still be executing the last move.
            // Enter the idle-wait state and poll '?' until GRBL reports <Idle|…>.
            if (_qCount == 0 && !_completePending) {
                _completePending   = true;
                _completePendingMs = millis();
                _idlePollMs        = millis() - 300; // trigger first poll immediately
                Log::nav("STR: all acked, polling for GRBL idle");
            }
            return;
        }

        uint8_t lineLen = (uint8_t)strlen(line);
        uint8_t sendLen = lineLen < kGrblBufSize ? lineLen + 1 : kGrblBufSize;

        if (_bufUsed + sendLen > kGrblBufSize || _qCount >= kQueueDepth) {
            strncpy(_pendingLine, line, sizeof(_pendingLine) - 1);
            _pendingLine[sizeof(_pendingLine) - 1] = '\0';
            _hasPendingLine = true;
            return;
        }

        Log::send(line);
        _grbl.print(line);
        _grbl.print('\n');   // '\n' only — println() adds '\r\n' (2 bytes) but sendLen only counts 1
        _qInjected[_qTail] = _pendingIsInjected;
        _pendingIsInjected = false;
        _pending[_qTail]   = sendLen;
        _qTail   = (_qTail + 1) % kQueueDepth;
        _qCount++;
        _bufUsed += sendLen;
        if (g_penDelayMs > 0 && isPenCommand(line)) {
            snprintf(_pendingLine, sizeof(_pendingLine), "G4 P%.3f", g_penDelayMs / 1000.0f);
            _hasPendingLine    = true;
            _pendingIsInjected = true;
        }
    }
}

void GCodeStreamer::handleGrblResponse() {
    char line[80];
    while (serialMgr.readLine(line, sizeof(line)))
        processGrblLine(line);
}

void GCodeStreamer::processGrblLine(const char *line) {
    Log::recv(line);
    if (_status == GCodeStatus::Resetting) {
        if (strstr(line, "Grbl") != nullptr) start(_filepath);
        return;
    }

    // Idle-wait: watching for GRBL to report <Idle|…> after all commands are acked.
    if (_completePending && line[0] == '<') {
        if (strstr(line, "Idle") != nullptr) {
            _completePending = false;
            completePlot();
        }
        return;  // status reports don't match ok/alarm/error — skip further checks
    }

    if (strcmp(line, "ok") == 0) {
        if (_qCount > 0) {
            _bufUsed -= _pending[_qHead];
            if (!_qInjected[_qHead]) _lineNumber++;
            _qHead = (_qHead + 1) % kQueueDepth;
            _qCount--;
        }
    } else if (strncmp(line, "ALARM:", 6) == 0) {
        _alarmCode = (int8_t)atoi(line + 6);
        _status = GCodeStatus::Alarm;
        _completePending = false;
        _qHead = 0; _qTail = 0; _qCount = 0; _bufUsed = 0;
        _hasPendingLine    = false;
        _pendingIsInjected = false;
        char msg[80];
        const char* base = strrchr(_filepath, '/');
        snprintf(msg, sizeof(msg), "ALARM:%d at line %lu in %s",
                 _alarmCode, (unsigned long)_lineNumber,
                 base ? base + 1 : _filepath);
        Log::err(msg);
    } else if (strncmp(line, "error", 5) == 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "error at line %lu: %s", (unsigned long)_lineNumber, line);
        Log::err(msg);
        _completePending = false;
        _status = GCodeStatus::Error;
    }
}

void GCodeStreamer::completePlot() {
    _src.close();
    _status = GCodeStatus::Completed;
    char msg[80];
    const char* base = strrchr(_filepath, '/');
    snprintf(msg, sizeof(msg), "complete: %lu lines in %s",
             (unsigned long)_lineNumber,
             base ? base + 1 : _filepath);
    Log::str(msg);
#ifdef ARDUINO
    char p[80]; progressPathFor(_filepath, p, sizeof(p));
    if (sd.exists(p)) sd.remove(p);
#endif
}

bool GCodeStreamer::saveProgress() {
#ifdef ARDUINO
    if (_filepath[0] == '\0') return false;
    char p[80]; progressPathFor(_filepath, p, sizeof(p));
    FsFile pf;
    if (!pf.open(p, O_WRONLY | O_CREAT | O_TRUNC)) return false;
    pf.print(_filepath);
    pf.print('\n');
    pf.print(_lineNumber);
    pf.println();
    pf.close();
    char msg[40]; snprintf(msg, sizeof(msg), "progress saved: line %lu", (unsigned long)_lineNumber);
    Log::str(msg);
    return true;
#else
    return false;
#endif
}

bool GCodeStreamer::loadProgress(uint32_t& lineOut) {
#ifdef ARDUINO
    if (_filepath[0] == '\0') return false;
    char p[80]; progressPathFor(_filepath, p, sizeof(p));
    if (!sd.exists(p)) return false;
    FsFile pf;
    if (!pf.open(p, O_READ)) return false;
    while (pf.available()) { if ((char)pf.read() == '\n') break; }
    char numBuf[12]; uint8_t numLen = 0;
    while (pf.available() && numLen < sizeof(numBuf) - 1) {
        char c = (char)pf.read();
        if (c == '\n' || c == '\r') break;
        numBuf[numLen++] = c;
    }
    numBuf[numLen] = '\0';
    lineOut = (uint32_t)atol(numBuf);
    pf.close();
    char msg[40]; snprintf(msg, sizeof(msg), "progress loaded: line %lu", (unsigned long)lineOut);
    Log::str(msg);
    return true;
#else
    return false;
#endif
}

void GCodeStreamer::trace(const char* filepath, Print& out) {
    if (!_src.open(filepath)) {
        out.print("TRACE ERROR: cannot open "); out.println(filepath);
        return;
    }

    char line[80];
    uint32_t kept = 0, skipped = 0;

    while (_src.available()) {
        _src.readLine(line, sizeof(line));
        char raw[80];
        strncpy(raw, line, sizeof(raw) - 1); raw[sizeof(raw) - 1] = '\0';

        if (!filterLine(line)) {
            out.print("   skip: "); out.println(raw);
            skipped++;
            continue;
        }
        injectMissingS(line, sizeof(line));
        applyPenOverwrite(line, sizeof(line));
        out.print("   send: "); out.println(line);
        kept++;
    }
    _src.close();

    // Final pen-lift the streamer appends after EOF
    if (g_overwriteS) snprintf(line, sizeof(line), "M4 S%d", g_penUpS);
    else              strncpy(line, "M4", sizeof(line));
    out.print("   send: "); out.println(line);
    kept++;

    char summary[64];
    snprintf(summary, sizeof(summary), "--- %u sent, %u filtered ---", (unsigned)kept, (unsigned)skipped);
    out.println(summary);
}
