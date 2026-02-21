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

    bool open(const char* path, const char* mode) {
        if (!_inited) return false;
        close(); 
        lock();
        _currentFile = SD_MMC.open(path, mode);
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

    bool readTextFileLimited(const char* path, char* dst, size_t maxLen) {
        if (!_inited) return false;

        File f = SD_MMC.open(path, FILE_READ);
        if (!f || f.isDirectory())
            return false;

        size_t size = f.size();
        if (size > maxLen) {
            f.close();
            return false;
        }

        f.readBytes(dst, size);
        dst[size] = 0;

        f.close();
        return true;
    }

    bool appendTextFile(const char* path, const char* text) {
        if (!_inited) return false;

        // FILE_APPEND поддерживается в FS.h, который использует SD_MMC
        File f = SD_MMC.open(path, FILE_APPEND);
        if (!f) return false;

        if (text && *text) {
            f.print(text);
            f.print("\n");
        }

        f.close();
        return true;
    }

    bool removeFile(const char* path) {
        if (!_inited) return false;
        // SD_MMC.remove делает проверку и удаление эффективнее
        return SD_MMC.remove(path);
    }    

    bool rmdirEmpty(const char* path) {
        if (!_inited) return false;

        File dir = SD_MMC.open(path);
        if (!dir || !dir.isDirectory())
            return false;

        // Проверяем, есть ли файлы внутри
        File entry = dir.openNextFile();
        if (entry) {
            // Директория не пуста
            entry.close();
            dir.close();
            return false;
        }

        dir.close();
        // Удаляем пустую директорию
        return SD_MMC.rmdir(path);
    }

    bool mkdir(const char* path) {
        if (!_inited) return false;
        return SD_MMC.mkdir(path);
    }

    bool readFile(const char* path, char* dst, size_t maxLen) {
        if (!_inited) return false;

        File f = SD_MMC.open(path, FILE_READ);
        if (!f || f.isDirectory()) return false;

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

    bool dirExists(const char* path) {
        if (!_inited) return false;

        File f = SD_MMC.open(path);
        if (!f) return false;

        bool isDir = f.isDirectory();
        f.close();
        return isDir;
    }

    size_t fileSize(const char* path) {
        if (!_inited) return 0;

        File f = SD_MMC.open(path, FILE_READ);
        if (!f) return 0;

        size_t size = f.size();
        f.close();
        return size;
    }

    uint64_t usedBytes() {
        if (!_inited) return 0;
        return SD_MMC.usedBytes();
    }    

    uint64_t totalBytes() { return _inited ? SD_MMC.totalBytes() : 0; }
    uint64_t freeBytes() { return _inited ? (SD_MMC.totalBytes() - SD_MMC.usedBytes()) : 0; }

    File* getFile() { return _currentFile ? &_currentFile : nullptr; }

}