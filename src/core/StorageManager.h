#ifndef POCKETDECK_STORAGE_MANAGER_H
#define POCKETDECK_STORAGE_MANAGER_H

#include <Arduino.h>

struct FileEntry {
    char     name[32];
    uint32_t size;
    bool     isDir;
};

class StorageManager {
public:
    StorageManager();

    bool begin();
    bool isMounted() const { return _mounted; }

    uint32_t totalBytes() const;
    uint32_t usedBytes() const;

    bool exists(const char* path);
    bool readFile(const char* path, char* outBuf, size_t maxLen);
    bool writeFile(const char* path, const char* content);
    bool deleteFile(const char* path);

    // List files inside a directory (e.g., "/" or "/notes"). Returns number of entries populated.
    int listDirectory(const char* dirPath, FileEntry* outEntries, int maxEntries);

private:
    void ensureDefaultFiles();
    bool _mounted;
};

extern StorageManager g_storage;

#endif // POCKETDECK_STORAGE_MANAGER_H
