#include "FileHandler.h"
#include <string.h>

// Returns true if this filename is a valid gcode file to show.
// Filters out macOS resource forks (._xxx) and hidden files.
// Accepts files ending in .gcode or .gco (case-insensitive).
static bool isGcodeFile(const char* name) {
  if (!name || name[0] == '.' || name[0] == '_') return false;
  size_t len = strlen(name);
  // Check .gcode (6 chars including dot)
  if (len >= 6) {
    const char* ext = name + len - 6;
    if (ext[0] == '.' &&
        (ext[1]=='g'||ext[1]=='G') && (ext[2]=='c'||ext[2]=='C') &&
        (ext[3]=='o'||ext[3]=='O') && (ext[4]=='d'||ext[4]=='D') &&
        (ext[5]=='e'||ext[5]=='E')) return true;
  }
  // Check .gco (4 chars including dot)
  if (len >= 4) {
    const char* ext = name + len - 4;
    if (ext[0] == '.' &&
        (ext[1]=='g'||ext[1]=='G') && (ext[2]=='c'||ext[2]=='C') &&
        (ext[3]=='o'||ext[3]=='O')) return true;
  }
  return false;
}

FileHandler::FileHandler()
  : maxFiles(50), fileCount(0), accessToFiles(false)
{}

FileHandler::~FileHandler() {
  // nothing to free anymore
}

void FileHandler::loadFileNames() {
  FsFile root = sd.open("/");
  if (!root) {
    accessToFiles = false;
    fileCount = 0;
    return;
  }
  root.rewind();
  accessToFiles = true;
  fileCount = 0;
  char nameBuf[64];
  while (true) {
    FsFile entry;
    if (!entry.openNext(&root, O_READ)) break;
    entry.getName(nameBuf, sizeof(nameBuf));
    if (!entry.isDir() && isGcodeFile(nameBuf)) {
      fileCount++;
      if (fileCount >= maxFiles) {
        entry.close();
        break;
      }
    }
    entry.close();
  }
  root.close();
}

int FileHandler::getFileCount() const {
  return fileCount;
}

bool FileHandler::getFileNameAt(int index, char* buf, size_t bufSize) const {
  if (index < 0 || index >= fileCount) {
    if (buf && bufSize) buf[0] = '\0';
    return false;
  }
  FsFile root = sd.open("/");
  if (!root) {
    if (buf && bufSize) buf[0] = '\0';
    return false;
  }
  root.rewind();
  int found = 0;
  bool ok = false;
  char nameBuf[64];
  while (true) {
    FsFile entry;
    if (!entry.openNext(&root, O_READ)) break;
    entry.getName(nameBuf, sizeof(nameBuf));
    if (!entry.isDir() && isGcodeFile(nameBuf)) {
      if (found == index) {
        strncpy(buf, nameBuf, bufSize - 1);
        buf[bufSize - 1] = '\0';
        ok = true;
        entry.close();
        break;
      }
      found++;
    }
    entry.close();
  }
  root.close();
  if (!ok && buf && bufSize) buf[0] = '\0';
  return ok;
}

String FileHandler::getFileName(int index) const {
  // Always fetch on demand. Use a temporary buffer sized reasonably.
  const size_t tmpSize = 128;
  char* tmp = new char[tmpSize];
  String res = "";
  if (getFileNameAt(index, tmp, tmpSize)) res = String(tmp);
  delete[] tmp;
  return res;
}

bool FileHandler::checkAccess() {
  if (!accessToFiles) {
    Serial.println("failed");
  } else {
    Serial.println("success");
  }
  return accessToFiles;
}
// ...existing code...