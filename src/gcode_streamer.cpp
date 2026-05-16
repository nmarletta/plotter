#include "gcode_streamer.h"
#include "logger.h"
#include "globals.h"

static String filterLine(const String &raw);        // forward declaration
static String applyPenOverwrite(const String &line); // forward declaration

GCodeStreamer::GCodeStreamer(HardwareSerial &grblSerial, uint8_t csPin)
    : _grbl(grblSerial), _sdCSPin(csPin) {}

bool GCodeStreamer::begin() {
    _status = GCodeStatus::Idle;
    return true;
}

bool GCodeStreamer::start(const char *filepath) {
    if (_status == GCodeStatus::Running) return false;
    _filepath = String(filepath);
    // reset queue and buffer state
    _qHead = 0; _qTail = 0; _qCount = 0; _bufUsed = 0;
    _rxLen = 0;
    _hasPendingLine = false;
    _sentFinalM5 = false;

    // Clear any GRBL alarm lock ($X). GRBL always responds "ok" to this,
    // even if no alarm is active, so it's safe to send unconditionally.
    _grbl.println("$X");
    unsigned long t = millis();
    while (millis() - t < 500) {
        if (_grbl.available()) {
            String r = _grbl.readStringUntil('\n');
            r.trim();
            if (r == "ok" || r.startsWith("error")) break;
        }
    }
    uint32_t resumeLine = 0;
    if (loadProgress(resumeLine)) {
        _lineNumber = resumeLine;
    } else {
        _lineNumber = 0;
    }
    if (!openFileAtLine(filepath, _lineNumber)) {
        _status = GCodeStatus::Error;
        return false;
    }

    // When resuming mid-file, GRBL has been reset and lost its modal state
    // (feed rate, units, distance mode). Re-send all setup lines from the
    // beginning of the file — anything before the first XY motion command.
    if (_lineNumber > 0) {
        FsFile pre;
        if (pre.open(filepath, O_READ)) {
            while (pre.available()) {
                String pLine = applyPenOverwrite(filterLine(readLineFromFile(pre)));
                if (pLine.length() == 0) continue;
                String up = pLine; up.toUpperCase();
                if (up.indexOf('X') >= 0 || up.indexOf('Y') >= 0) break; // reached motion — stop
                _grbl.println(pLine);
                // Wait briefly for ok before sending next preamble line
                unsigned long tw = millis();
                while (millis() - tw < 500) {
                    if (_grbl.available()) {
                        String r = _grbl.readStringUntil('\n'); r.trim();
                        if (r == "ok" || r.startsWith("error")) break;
                    }
                }
            }
            pre.close();
        }
    }
    _fileSize = _file.size();
    _status = GCodeStatus::Running;
    _lastSaveMs = millis();
    return true; // pumpLines() called from tick()
}

void GCodeStreamer::pause() {
    if (_status == GCodeStatus::Running) {
        _grbl.write('!'); // GRBL real-time feed hold — stops motion immediately
        _status = GCodeStatus::Paused;
    }
}

void GCodeStreamer::resume() {
    if (_status == GCodeStatus::Paused) {
        _grbl.write('~'); // GRBL real-time cycle start — resumes motion
        _status = GCodeStatus::Running;
        pumpLines();
    }
}

void GCodeStreamer::cancel() {
    _grbl.write(0x18); // GRBL soft reset: clears planner buffer and disables spindle (= pen up)
    _status = GCodeStatus::Canceled;
    _qHead = 0; _qTail = 0; _qCount = 0; _bufUsed = 0;
    _hasPendingLine = false;
    String p = progressPathFor(_filepath);
    if (sd.exists(p.c_str())) sd.remove(p.c_str());
    if (_file) _file.close();
}

void GCodeStreamer::retryAfterError() {
    _grbl.write(0x18); // soft reset GRBL: pen up, buffer cleared
    _status = GCodeStatus::Resetting;
    _resetStartMs = millis();
    _qHead = 0; _qTail = 0; _qCount = 0; _bufUsed = 0;
    _hasPendingLine = false;
    if (_file) _file.close();
    // progress file intentionally kept — start() will resume from saved line
}

void GCodeStreamer::stop() {
    if (_file) _file.close();
    _status = GCodeStatus::Idle;
}

GCodeStatus GCodeStreamer::status() const { return _status; }
uint32_t GCodeStreamer::currentLine() const { return _lineNumber; }
int8_t GCodeStreamer::alarmCode() const { return _alarmCode; }
float GCodeStreamer::progress() const {
    if (_status == GCodeStatus::Completed) return 1.0f;
    if (_fileSize == 0) return 0.0f;
    return constrain((float)_file.curPosition() / (float)_fileSize, 0.0f, 1.0f);
}
String GCodeStreamer::currentFilename() const { return _filepath; }

void GCodeStreamer::unlock() {
    // Send $X (alarm clear) and wait briefly for GRBL ok
    _grbl.println("$X");
    unsigned long t = millis();
    while (millis() - t < 500) {
        if (_grbl.available()) {
            String r = _grbl.readStringUntil('\n');
            r.trim();
            if (r == "ok" || r.startsWith("error")) break;
        }
    }
    // Reset queue — GRBL cleared its planner buffer on alarm
    _qHead = 0; _qTail = 0; _qCount = 0; _bufUsed = 0;
    _hasPendingLine = false;
    _alarmCode = 0;
    // Reopen file at the last acknowledged line so resume continues from there
    openFileAtLine(_filepath.c_str(), _lineNumber);
    _status = GCodeStatus::Paused;
}

void GCodeStreamer::tick() {
    if (_status == GCodeStatus::Idle || _status == GCodeStatus::Completed || _status == GCodeStatus::Error) return;
    handleGrblResponse();

    if (_status == GCodeStatus::Paused || _status == GCodeStatus::Canceled || _status == GCodeStatus::Alarm) return;

    if (_status == GCodeStatus::Resetting) {
        if (millis() - _resetStartMs > kResetTimeoutMs) {
            _status = GCodeStatus::Error; // GRBL never responded after reset
        }
        return;
    }

    // Periodic progress save
    if (millis() - _lastSaveMs > kSaveIntervalMs) {
        saveProgress();
        _lastSaveMs = millis();
    }

    pumpLines();
}

bool GCodeStreamer::openFileAtLine(const char *path, uint32_t lineToStart) {
    if (_file) _file.close();
    if (!_file.open(path, O_READ)) return false;
    _file.seek(0);
    for (uint32_t i = 0; i < lineToStart; ++i) {
        if (!_file.available()) break;
        readLineFromFile(_file);
    }
    return true;
}

String GCodeStreamer::readLineFromFile(FsFile &f) {
    String line;
    while (f.available()) {
        char c = f.read();
        if (c == '\r') continue;
        if (c == '\n') break;
        line += c;
    }
    // trim trailing/leading spaces
    line.trim();
    return line;
}

// Filter a raw gcode line for this plotter:
//   - Returns "" for lines that should be skipped entirely.
//   - Returns the cleaned line otherwise.
// Rules (applied in order):
//   1. Strip inline (...) comment from end of line.
//   2. Skip blank lines, ; comments, ( comments, T tool-change lines.
//   3. Z with no X/Y: the whole line is a pen up/down command — skip it.
//   4. Z alongside X/Y: strip the Z token, preserve the lateral move.
static String filterLine(const String &raw) {
    // 1. strip inline comment
    String line = raw;
    int cp = line.indexOf('(');
    if (cp >= 0) { line = line.substring(0, cp); line.trim(); }

    // 2. skip non-motion lines
    if (line.length() == 0) return "";
    if (line.startsWith(";") || line.startsWith("(")) return "";
    if (line.startsWith("T") || line.startsWith("t")) return "";

    // 3 & 4. handle Z axis
    String up = line; up.toUpperCase();
    int z = up.indexOf('Z');
    if (z >= 0) {
        if (up.indexOf('X') < 0 && up.indexOf('Y') < 0) return ""; // no X/Y: Z-only move, no Z axis on this machine — skip
        // Z alongside XY: strip the Z<value> token
        int end = z + 1;
        if (end < (int)line.length() && (line[end] == '-' || line[end] == '+')) end++;
        while (end < (int)line.length() && (isDigit(line[end]) || line[end] == '.')) end++;
        if (end < (int)line.length() && line[end] == ' ') end++;
        line = line.substring(0, z) + line.substring(end);
        line.trim();
    }

    return line;
}

// Always inject S values for bare M4/M5 commands (no S present).
// Prevents GRBL from receiving M4/M5 without a servo position, which causes undefined behaviour.
static String injectMissingS(const String &line) {
    String up = line; up.toUpperCase();
    if (up.indexOf('X') >= 0 || up.indexOf('Y') >= 0) return line; // motion line — leave alone
    if (up.indexOf('S') >= 0) return line;                          // already has S value
    char buf[16];
    if (up.indexOf("M4") >= 0) {
        snprintf(buf, sizeof(buf), "M4 S%d", g_penUpS);
        return String(buf);
    }
    if (up.indexOf("M5") >= 0) {
        snprintf(buf, sizeof(buf), "M5 S%d", g_penDownS);
        return String(buf);
    }
    return line;
}

// Rewrite standalone M3/M5 lines to use configured S values when overwrite is enabled.
static String applyPenOverwrite(const String &line) {
    if (!g_overwriteS) return line;
    String up = line; up.toUpperCase();
    if (up.indexOf('X') >= 0 || up.indexOf('Y') >= 0) return line; // motion line — leave alone
    if (up.indexOf("M3") >= 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "M5 S%d", g_penDownS); // M5 Sxxx = pen down in grbl-servo
        return String(buf);
    }
    if (up.indexOf("M5") >= 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "M4 S%d", g_penUpS); // M4 Sxxx = pen up in grbl-servo
        return String(buf);
    }
    return line;
}

void GCodeStreamer::pumpLines() {
    while (_status == GCodeStatus::Running) {
        String line;

        if (_hasPendingLine) {
            // A line was read but the buffer was full last tick — retry
            line = _pendingLine;
            _hasPendingLine = false;
        } else if (_file && _file.available()) {
            line = filterLine(readLineFromFile(_file));
            if (line.length() == 0) { _lineNumber++; continue; }
            line = injectMissingS(line);
            line = applyPenOverwrite(line);
        } else if (!_sentFinalM5) {
            // File exhausted — send one defensive pen-up before completing
            if (g_overwriteS) {
                char buf[16];
                snprintf(buf, sizeof(buf), "M4 S%d", g_penUpS); // M4 Sxxx = pen up in grbl-servo
                line = String(buf);
            } else {
                line = "M4"; // default pen-up command
            }
            _sentFinalM5 = true;
        } else {
            // All lines sent — wait for remaining oks
            if (_qCount == 0) {
                _file.close();
                _status = GCodeStatus::Completed;
                // Delete the progress file — plot finished cleanly.
                // (If we saved it, the next run of the same file would resume from EOF and skip everything.)
                String p = progressPathFor(_filepath);
                if (sd.exists(p.c_str())) sd.remove(p.c_str());
            }
            return;
        }

        uint8_t sendLen = (line.length() < kGrblBufSize)
            ? (uint8_t)(line.length() + 1) : kGrblBufSize;

        if (_bufUsed + sendLen > kGrblBufSize || _qCount >= kQueueDepth) {
            // GRBL's buffer is full — hold this line until space frees up
            _pendingLine = line;
            _hasPendingLine = true;
            return;
        }

        Log::send(line.c_str());
        _grbl.println(line);
        _pending[_qTail] = sendLen;
        _qTail = (_qTail + 1) % kQueueDepth;
        _qCount++;
        _bufUsed += sendLen;
    }
}

void GCodeStreamer::handleGrblResponse() {
    while (_grbl.available()) {
        char c = (char)_grbl.read();
        if (c == '\r') continue;
        if (c == '\n') {
            _rxBuf[_rxLen] = '\0';
            if (_rxLen > 0) processGrblLine(_rxBuf);
            _rxLen = 0;
        } else if (_rxLen < sizeof(_rxBuf) - 1) {
            _rxBuf[_rxLen++] = c;
        }
    }
}

void GCodeStreamer::processGrblLine(const char *line) {
    Log::recv(line);
    if (_status == GCodeStatus::Resetting) {
        if (strstr(line, "Grbl") != nullptr) {
            start(_filepath.c_str()); // resumes from saved progress line
        }
        return;
    }

    if (strcmp(line, "ok") == 0) {
        if (_qCount > 0) {
            _bufUsed -= _pending[_qHead];
            _qHead = (_qHead + 1) % kQueueDepth;
            _qCount--;
            _lineNumber++;
            // progress saved on timer in tick(), not per-ok
        }
    } else if (strncmp(line, "ALARM:", 6) == 0) {
        _alarmCode = (int8_t)atoi(line + 6);
        _status = GCodeStatus::Alarm;
        // Reset queue — GRBL clears its buffer on alarm
        _qHead = 0; _qTail = 0; _qCount = 0; _bufUsed = 0;
        _hasPendingLine = false;
    } else if (strncmp(line, "error", 5) == 0) {
        _status = GCodeStatus::Error;
    }
    // status reports (<...>) and other GRBL chatter silently ignored
}

bool GCodeStreamer::saveProgress() {
    if (_filepath.length() == 0) return false;
    String p = progressPathFor(_filepath);
    FsFile pf;
    if (!pf.open(p.c_str(), O_WRONLY | O_CREAT | O_TRUNC)) return false;
    pf.print(_filepath);
    pf.print('\n');
    pf.print(_lineNumber);
    pf.println();
    pf.close();
    return true;
}

bool GCodeStreamer::loadProgress(uint32_t &lineOut) {
    if (_filepath.length() == 0) return false;
    String p = progressPathFor(_filepath);
    if (!sd.exists(p.c_str())) return false;
    FsFile pf;
    if (!pf.open(p.c_str(), O_READ)) return false;
    pf.readStringUntil('\n'); // skip filepath line
    String lineStr = pf.readStringUntil('\n');
    lineStr.trim();
    lineOut = (uint32_t)lineStr.toInt();
    pf.close();
    return true;
}

String GCodeStreamer::progressPathFor(const String &gcodePath) const {
    // create a deterministic filename for progress; simple approach: append ".progress"
    // If gcodePath contains folders, create same folder or store in root; here store in SD root using basename
    int ix = gcodePath.lastIndexOf('/');
    String base = (ix >= 0) ? gcodePath.substring(ix + 1) : gcodePath;
    return String("/") + base + ".progress";
}