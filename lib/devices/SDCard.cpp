#include "SDCard.h"
#include "SD_MMC.h"
#include "FS.h"
#include "LOG.h"

// ===== SD_MMC 1-bit pins for ESP32-S3 =====
#define SD_MMC_CLK  18
#define SD_MMC_CMD  17
#define SD_MMC_D0   16

static bool sd_mmc_global_inited = false;

SDCard::SDCard() : inited_(false), currentFile_(nullptr) {
}

SDCard::~SDCard() {
    close();
}

bool SDCard::init() {
    LOG.println("Go SDCard init");

    if (sd_mmc_global_inited) {
        inited_ = true;
        return true;
    }

    if (!SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0)) {
        LOG.println("SD_MMC.setPins - false");
        return false;
    }

    LOG.println("SD_MMC.setPins - true");

    if (!SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_HIGHSPEED)) {
        LOG.println("SD_MMC.begin - false");
        return false;
    }

    LOG.println("SD_MMC.begin - true");

    sd_mmc_global_inited = true;
    inited_ = true;
    return true;
}

bool SDCard::open(const char* path) {
    if (!inited_) return false;

    close();

    currentFile_ = SD_MMC.open(path, FILE_READ);
    if (!currentFile_ || currentFile_.isDirectory()) {
        currentFile_ = File();
        return false;
    }

    return true;
}

size_t SDCard::read(void* dst, size_t len) {
    if (!currentFile_) return 0;
    return currentFile_.read((uint8_t*)dst, len);
}

bool SDCard::available() {
    return currentFile_ && currentFile_.available();
}

void SDCard::close() {
    if (currentFile_) {
        currentFile_.close();
        currentFile_ = File();
    }
}

bool SDCard::readFile(const char* path, char* dst, size_t maxLen) {
    if (!inited_) return false;

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

void SDCard::listDir(const char* path, void (*callback)(void* user, const char* name, bool isDir), void* user) {
    if (!inited_) return;

    close();

    File dir = SD_MMC.open(path);
    if (!dir || !dir.isDirectory())
        return;

    // Предполагается, что структура DirEntry и константа MAX_DIR_ENTRIES 
    // определены в SDCard.h, как в твоем оригинале
    DirEntry entries[MAX_DIR_ENTRIES];
    int count = 0;

    dir.rewindDirectory();

    File entry;
    while ((entry = dir.openNextFile()) && count < MAX_DIR_ENTRIES) {
        strncpy(entries[count].name, entry.name(), DirEntry::MAX_NAME_LEN);
        entries[count].name[DirEntry::MAX_NAME_LEN - 1] = 0;
        entries[count].isDir = entry.isDirectory();
        count++;
        entry.close();
    }
    dir.close();

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

    File f = SD_MMC.open(path);
    if (!f) return false;

    bool isDir = f.isDirectory();
    f.close();
    return isDir;
}

bool SDCard::fileExists(const char* path) {
    if (!inited_) return false;

    // Более оптимальный метод проверки существования в SD_MMC
    return SD_MMC.exists(path);
}

size_t SDCard::fileSize(const char* path) {
    if (!inited_) return 0;

    File f = SD_MMC.open(path, FILE_READ);
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

    if (outSize)
        *outSize = size;

    f.close();
    return true;
}

bool SDCard::mkdir(const char* path) {
    if (!inited_) return false;
    return SD_MMC.mkdir(path);
}

bool SDCard::rmdirEmpty(const char* path) {
    if (!inited_) return false;

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

bool SDCard::removeFile(const char* path) {
    if (!inited_) return false;
    // SD_MMC.remove делает проверку и удаление эффективнее
    return SD_MMC.remove(path);
}

bool SDCard::writeTextFile(const char* path, const char* text) {
    if (!inited_) return false;

    if (SD_MMC.exists(path)) {
        SD_MMC.remove(path);
    }

    File f = SD_MMC.open(path, FILE_WRITE);
    if (!f) return false;

    if (text && *text) {
        f.print(text);
        f.print("\n"); // Добавляем перенос строки
    }

    f.close();
    return true;
}

bool SDCard::appendTextFile(const char* path, const char* text) {
    if (!inited_) return false;

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