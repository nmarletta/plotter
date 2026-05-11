// ...existing code...
#pragma once
#include <SdFat.h>
#include <Arduino.h>

extern SdFat sd;

class FileHandler {
public:
    FileHandler();
    ~FileHandler();

    void loadFileNames();
    int getFileCount() const;
    bool getFileNameAt(int index, char* buf, size_t bufSize) const;
    String getFileName(int index) const;
    bool checkAccess();

private:
    int maxFiles;
    int fileCount;
    bool accessToFiles;

    // Cache/buffer removed — always fetch on demand
    // ...existing code...
};