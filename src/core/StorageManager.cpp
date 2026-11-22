#include "StorageManager.h"
#include <FS.h>
#include <LittleFS.h>
#include <string.h>
#include <stdio.h>

StorageManager g_storage;

StorageManager::StorageManager()
    : _mounted(false) {}

bool StorageManager::begin() {
    if (_mounted) return true;
    // Mount LittleFS; format only if partition has never been formatted
    _mounted = LittleFS.begin(true);
    if (_mounted) {
        ensureDefaultFiles();
    }
    return _mounted;
}

void StorageManager::ensureDefaultFiles() {
    if (!LittleFS.exists("/notes")) {
        LittleFS.mkdir("/notes");
    }
    if (!LittleFS.exists("/readme.txt")) {
        writeFile("/readme.txt", "PocketDeck OS v1.0\nESP32 Mini Cyberdeck\nOffline Utility System");
    }
    if (!LittleFS.exists("/notes/welcome.txt")) {
        writeFile("/notes/welcome.txt", "Welcome to PocketDeck! Use keypad multi-tap to edit notes.");
    }
}

uint32_t StorageManager::totalBytes() const {
    if (!_mounted) return 0;
    return (uint32_t)LittleFS.totalBytes();
}

uint32_t StorageManager::usedBytes() const {
    if (!_mounted) return 0;
    return (uint32_t)LittleFS.usedBytes();
}

bool StorageManager::exists(const char* path) {
    if (!_mounted || !path) return false;
    return LittleFS.exists(path);
}

bool StorageManager::readFile(const char* path, char* outBuf, size_t maxLen) {
    if (!_mounted || !path || !outBuf || maxLen == 0) return false;
    outBuf[0] = '\0';

    File f = LittleFS.open(path, FILE_READ);
    if (!f || f.isDirectory()) {
        return false;
    }

    size_t toRead = f.size();
    if (toRead >= maxLen) {
        toRead = maxLen - 1;
    }
    size_t bytesRead = f.readBytes(outBuf, toRead);
    outBuf[bytesRead] = '\0';
    f.close();
    return true;
}

bool StorageManager::writeFile(const char* path, const char* content) {
    if (!_mounted || !path || !content) return false;

    File f = LittleFS.open(path, FILE_WRITE);
    if (!f) {
        return false;
    }
    size_t len = strlen(content);
    size_t written = f.write((const uint8_t*)content, len);
    f.close();
    return (written == len);
}

bool StorageManager::deleteFile(const char* path) {
    if (!_mounted || !path) return false;
    if (!LittleFS.exists(path)) return false;
    return LittleFS.remove(path);
}

int StorageManager::listDirectory(const char* dirPath, FileEntry* outEntries, int maxEntries) {
    if (!_mounted || !dirPath || !outEntries || maxEntries <= 0) return 0;

    File dir = LittleFS.open(dirPath);
    if (!dir || !dir.isDirectory()) {
        return 0;
    }

    int count = 0;
    File entry = dir.openNextFile();
    while (entry && count < maxEntries) {
        const char* rawName = entry.name();
        // Extract basename if full path is returned
        const char* slash = strrchr(rawName, '/');
        const char* baseName = slash ? (slash + 1) : rawName;

        if (baseName[0] != '\0') {
            strncpy(outEntries[count].name, baseName, sizeof(outEntries[count].name) - 1);
            outEntries[count].name[sizeof(outEntries[count].name) - 1] = '\0';
            outEntries[count].size  = (uint32_t)entry.size();
            outEntries[count].isDir = entry.isDirectory();
            count++;
        }
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();
    return count;
}
