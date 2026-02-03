#pragma once

#include <stdint.h>
#include <stddef.h>
#include <FS.h>

namespace fs {
    class File;
}

class SdReadBuffer {
public:
    explicit SdReadBuffer(fs::File* file);

    uint8_t  readU8();
    uint16_t readU16();
    void readBytes(void* dst, size_t len);
    bool available() const;

private:
    fs::File* _file;

    static constexpr size_t BUF_SIZE = 8192 * 2; // 8kb
    uint8_t _buf[BUF_SIZE];
    size_t _pos = 0;
    size_t _size = 0;

    void refill();
    void ensure(size_t needed);
};
