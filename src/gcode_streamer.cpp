#include "gcode_streamer.h"

GCodeStreamer::GCodeStreamer(HardwareSerial &grblSerial, uint8_t csPin)
    : _grbl(grblSerial), _sdCSPin(csPin) {}

bool GCodeStreamer::begin() {
    _status = GCodeStatus::Idle;
    return true;
}

bool GCodeStreamer::start(const char *filepath) {
    if (_status == GCodeStatus::Running || _status == GCodeStatus::WaitingForOk) return false;
    _filepath = String(filepath);
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
    _status = GCodeStatus::Running;
    _waitingForOk = false;
    // ensure serial at correct baud is initialized externally
    return sendNextLine(); // attempt to send first line
}

void GCodeStreamer::pause() {
    if (_status == GCodeStatus::Running || _status == GCodeStatus::WaitingForOk) {
        _status = GCodeStatus::Paused;
    }
}

void GCodeStreamer::resume() {
    if (_status == GCodeStatus::Paused) {
        _status = GCodeStatus::Running;
        // if not waiting, try send next
        if (!_waitingForOk) sendNextLine();
    }
}

void GCodeStreamer::cancel() {
    _status = GCodeStatus::Canceled;
    // remove progress file
    String p = progressPathFor(_filepath);
    if (sd.exists(p.c_str())) sd.remove(p.c_str());
    if (_file) _file.close();
}

void GCodeStreamer::stop() {
    if (_file) _file.close();
    _status = GCodeStatus::Idle;
}

GCodeStatus GCodeStreamer::status() const { return _status; }
uint32_t GCodeStreamer::currentLine() const { return _lineNumber; }
String GCodeStreamer::currentFilename() const { return _filepath; }

void GCodeStreamer::tick() {
    if (_status == GCodeStatus::Idle || _status == GCodeStatus::Completed || _status == GCodeStatus::Error) return;
    // read incoming GRBL serial for responses
    handleGrblResponse();

    if (_status == GCodeStatus::Paused || _status == GCodeStatus::Canceled) return;

    // periodically save progress
    if (millis() - _lastProgressSaveMs > _progressSaveIntervalMs && _status == GCodeStatus::Running) {
        saveProgress();
        _lastProgressSaveMs = millis();
    }

    // if not waiting for ok, try to send next
    if (!_waitingForOk && _status == GCodeStatus::Running) {
        sendNextLine();
    }
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

bool GCodeStreamer::sendNextLine() {
    if (!_file) {
        _status = GCodeStatus::Error;
        return false;
    }
    while (_file.available()) {
        String line = readLineFromFile(_file);
        // skip blank lines and comments (Grbl comments often start with ';' or '(')
        if (line.length() == 0) { _lineNumber++; continue; }
        if (line.startsWith(";") || line.startsWith("(")) { _lineNumber++; continue; }
        // send the line to GRBL
        _grbl.println(line);
        _waitingForOk = true;
        _status = GCodeStatus::WaitingForOk;
        // note: do NOT increment _lineNumber until we receive 'ok' (so resume works)
        return true;
    }
    // no more lines
    _status = GCodeStatus::Completed;
    saveProgress(); // final save (may remove progress file if desired)
    return true;
}

void GCodeStreamer::handleGrblResponse() {
    if (!_grbl) return;
    while (_grbl.available()) {
        String resp = _grbl.readStringUntil('\n');
        resp.trim();
        if (resp.length() == 0) continue;
        // GRBL responses: "ok", "error:...", "ALARM:..." and status reports. Accept substring "ok".
        if (resp.indexOf("ok") >= 0) {
            if (_waitingForOk) {
                _waitingForOk = false;
                // We consider the previously sent line as completed -> increment
                _lineNumber++;
                _status = GCodeStatus::Running;
                saveProgress();
            }
        } else if (resp.indexOf("error") >= 0 || resp.indexOf("ALARM") >= 0) {
            // On error, mark and pause so user can decide
            _status = GCodeStatus::Error;
            _waitingForOk = false;
        } else {
            // other GRBL chatter; ignore or hook for logging
        }
    }
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
    String storedPath = pf.readStringUntil('\n');
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