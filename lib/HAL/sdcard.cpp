#include "sdcard.h"
#include "SD_MMC.h"
#include "LOG.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define SD_MMC_CLK  18
#define SD_MMC_CMD  17
#define SD_MMC_D0   16

namespace SDCARD {

namespace { // Приватные данные синглтона
    bool _inited = false;
    File _currentFile;
    SemaphoreHandle_t _sdMutex = nullptr;
    static constexpr int MAX_DIR_ENTRIES = 64;
}

void lock() { if (_sdMutex) xSemaphoreTake(_sdMutex, portMAX_DELAY); }
void unlock() { if (_sdMutex) xSemaphoreGive(_sdMutex); }

bool init() {
    if (_sdMutex == nullptr) _sdMutex = xSemaphoreCreateMutex();
    
    lock();
    if (_inited) { unlock(); return true; }

    LOG.println("SDCard: Hardware init...");

    if (!SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0)) {
        LOG.println("SDCard: SetPins failed");
        unlock();
        return false;
    }

    // Для ESP32-S3 часто лучше использовать false в mode1bit (2-й параметр begin)
    if (!SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_HIGHSPEED)) {
        LOG.println("SDCard: Mount failed");
        unlock();
        return false;
    }

    _inited = true;
    LOG.println("SDCard: Ready");
    unlock();
    return true;
}

bool open(const char* path) {
    if (!_inited) return false;
    close(); 
    lock();
    _currentFile = SD_MMC.open(path, FILE_READ);
    if (!_currentFile || _currentFile.isDirectory()) {
        _currentFile = File();
        unlock();
        return false;
    }
    unlock();
    return true;
}

size_t read(void* dst, size_t len) {
    if (!_currentFile) return 0;
    lock();
    size_t r = _currentFile.read((uint8_t*)dst, len);
    unlock();
    return r;
}

bool available() {
    return _currentFile && _currentFile.available();
}

void close() {
    lock();
    if (_currentFile) {
        _currentFile.close();
        _currentFile = File();
    }
    unlock();
}

bool fileExists(const char* path) {
    if (!_inited) return false;
    lock();
    bool e = SD_MMC.exists(path);
    unlock();
    return e;
}

void listDir(const char* path, void (*callback)(void* user, const char* name, bool isDir), void* user) {
    if (!_inited) return;
    
    lock();
    File dir = SD_MMC.open(path);
    if (!dir || !dir.isDirectory()) {
        unlock();
        return;
    }

    DirEntry entries[MAX_DIR_ENTRIES];
    int count = 0;

    File entry;
    while ((entry = dir.openNextFile()) && count < MAX_DIR_ENTRIES) {
        strncpy(entries[count].name, entry.name(), DirEntry::MAX_NAME_LEN);
        entries[count].name[DirEntry::MAX_NAME_LEN - 1] = 0;
        entries[count].isDir = entry.isDirectory();
        count++;
        entry.close();
    }
    dir.close();
    unlock(); // Освобождаем SD для других задач, пока сортируем

    // Сортировка
    for (int i = 0; i < count - 1; ++i) {
        for (int j = i + 1; j < count; ++j) {
            bool swap = (entries[i].isDir != entries[j].isDir) ? !entries[i].isDir : (strcasecmp(entries[i].name, entries[j].name) > 0);
            if (swap) { DirEntry tmp = entries[i]; entries[i] = entries[j]; entries[j] = tmp; }
        }
    }

    for (int i = 0; i < count; ++i) callback(user, entries[i].name, entries[i].isDir);
}

bool writeTextFile(const char* path, const char* text) {
    if (!_inited) return false;
    lock();
    if (SD_MMC.exists(path)) SD_MMC.remove(path);
    File f = SD_MMC.open(path, FILE_WRITE);
    if (!f) { unlock(); return false; }
    if (text) { f.print(text); f.print("\n"); }
    f.close();
    unlock();
    return true;
}

uint64_t totalBytes() { return _inited ? SD_MMC.totalBytes() : 0; }
uint64_t freeBytes() { return _inited ? (SD_MMC.totalBytes() - SD_MMC.usedBytes()) : 0; }

File* getFile() { return _currentFile ? &_currentFile : nullptr; }

}