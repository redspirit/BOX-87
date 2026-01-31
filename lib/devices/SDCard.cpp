#include "SDCard.h"
#include <SD.h>
#include <SPI.h>

// ===== SD SPI pins =====
#define SD_CS    21
#define SD_SCK   18
#define SD_MOSI  17
#define SD_MISO  16

SDCard::SDCard() {
}

SDCard::~SDCard() {
    close();
}

bool SDCard::init() {
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    inited_ = SD.begin(SD_CS);
    return inited_;
}

bool SDCard::open(const char* path) {
    if (!inited_) return false;

    close();
    fs::File f = SD.open(path, FILE_READ);
    if (!f)
        return false;

    currentFile_ = new fs::File(f);
    return true;
}

size_t SDCard::read(void* dst, size_t len) {
    if (!currentFile_) return 0;
    return currentFile_->read((uint8_t*)dst, len);
}

bool SDCard::available() {
    return currentFile_ && currentFile_->available();
}

void SDCard::close() {
    if (currentFile_) {
        currentFile_->close();
        delete currentFile_;
        currentFile_ = nullptr;
    }
}

bool SDCard::readFile(const char* path, char* dst, size_t maxLen) {
    if (!inited_) return false;

    fs::File f = SD.open(path, FILE_READ);
    if (!f) return false;

    size_t size = f.size();
    if (size >= maxLen) {
        f.close();
        return false;
    }

    f.readBytes(dst, size);
    dst[size] = 0;
    f.close();
    return true;
}

void SDCard::listDir(const char* path, void (*callback)(void* user, const char* name, bool isDir), void* user) {
    if (!inited_) return;

    fs::File dir = SD.open(path);
    if (!dir || !dir.isDirectory())
        return;

    DirEntry entries[MAX_DIR_ENTRIES];
    int count = 0;

    fs::File entry;
    while ((entry = dir.openNextFile()) && count < MAX_DIR_ENTRIES) {
        strncpy(entries[count].name, entry.name(), DirEntry::MAX_NAME_LEN);
        entries[count].name[DirEntry::MAX_NAME_LEN - 1] = 0;
        entries[count].isDir = entry.isDirectory();
        count++;
        entry.close();
    }
    dir.close();

    // сортировка: директории → файлы, затем по имени
    for (int i = 0; i < count - 1; ++i) {
        for (int j = i + 1; j < count; ++j) {
            bool swap = false;

            if (entries[i].isDir != entries[j].isDir) {
                if (!entries[i].isDir)
                    swap = true;
            } else {
                if (strcasecmp(entries[i].name, entries[j].name) > 0)
                    swap = true;
            }

            if (swap) {
                DirEntry tmp = entries[i];
                entries[i] = entries[j];
                entries[j] = tmp;
            }
        }
    }

    for (int i = 0; i < count; ++i) {
        callback(user, entries[i].name, entries[i].isDir);
    }
}

bool SDCard::dirExists(const char* path) {
    if (!inited_) return false;

    fs::File f = SD.open(path);
    if (!f) return false;

    bool isDir = f.isDirectory();
    f.close();
    return isDir;
}

bool SDCard::fileExists(const char* path) {
    if (!inited_) return false;

    fs::File f = SD.open(path, FILE_READ);
    if (!f) return false;

    bool ok = !f.isDirectory();
    f.close();
    return ok;
}

size_t SDCard::fileSize(const char* path) {
    if (!inited_) return 0;

    fs::File f = SD.open(path, FILE_READ);
    if (!f) return 0;

    size_t size = f.size();
    f.close();
    return size;
}

bool SDCard::readTextFileLimited(
    const char* path,
    char* dst,
    size_t maxLen,
    size_t* outSize
) {
    if (!inited_) return false;

    fs::File f = SD.open(path, FILE_READ);
    if (!f || f.isDirectory())
        return false;

    size_t size = f.size();
    if (size > maxLen) {
        f.close();
        return false;
    }

    f.readBytes(dst, size);
    dst[size] = 0;

    if (outSize)
        *outSize = size;

    f.close();
    return true;
}

bool SDCard::mkdir(const char* path) {
    if (!inited_) return false;
    return SD.mkdir(path);
}

bool SDCard::rmdirEmpty(const char* path) {
    if (!inited_) return false;

    fs::File dir = SD.open(path);
    if (!dir || !dir.isDirectory())
        return false;

    fs::File entry = dir.openNextFile();
    if (entry) {
        entry.close();
        dir.close();
        return false;
    }

    dir.close();
    return SD.rmdir(path);
}

bool SDCard::removeFile(const char* path) {
    if (!inited_) return false;

    fs::File f = SD.open(path);
    if (!f || f.isDirectory()) {
        if (f) f.close();
        return false;
    }

    f.close();
    return SD.remove(path);
}

bool SDCard::writeTextFile(const char* path, const char* text) {
    if (!inited_) return false;

    if (SD.exists(path)) {
        SD.remove(path);
    }

    fs::File f = SD.open(path, FILE_WRITE);
    if (!f) return false;

    if (text && *text) {
        f.print(text);
        f.print("\n");
    }

    f.close();
    return true;
}

bool SDCard::appendTextFile(const char* path, const char* text) {
    if (!inited_) return false;

    fs::File f = SD.open(path, FILE_WRITE);
    if (!f) return false;

    f.seek(f.size());

    if (text && *text) {
        f.print(text);
        f.print("\n");
    }

    f.close();
    return true;
}
