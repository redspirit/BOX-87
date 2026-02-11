#pragma once

#include <stdint.h>
#include <cstddef>

#include <FS.h>

class SDCard {
public:
    SDCard();
    ~SDCard();

    bool init();

    bool open(const char* path);
    size_t read(void* dst, size_t len);
    bool available();
    void close();

    bool readFile(const char* path, char* dst, size_t maxLen);
    void listDir(const char* path, void (*callback)(void* user, const char* name, bool isDir), void* user);
    bool dirExists(const char* path);

    bool mkdir(const char* path);
    bool rmdirEmpty(const char* path);
    bool removeFile(const char* path);

    bool fileExists(const char* path);
    size_t fileSize(const char* path);

    bool writeTextFile(const char* path, const char* text);
    bool appendTextFile(const char* path, const char* text);

    uint64_t totalBytes();
    uint64_t usedBytes();
    uint64_t freeBytes();

    File* file() { return currentFile_.operator bool() ? &currentFile_ : nullptr; }
    
    bool readTextFileLimited(
        const char* path,
        char* dst,
        size_t maxLen,
        size_t* outSize = nullptr
    );

private:
    bool inited_ = false;

    struct DirEntry {
        static constexpr int MAX_NAME_LEN = 32;
        char name[MAX_NAME_LEN];
        bool isDir;
    };

    static constexpr int MAX_DIR_ENTRIES = 64;

private:
    File currentFile_;
};
