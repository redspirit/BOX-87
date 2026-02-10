#include "SdReadBuffer.h"
#include <string.h>
#include <esp_heap_caps.h>

SdReadBuffer::SdReadBuffer(fs::File* file)
    : _file(file),
      _buf(nullptr),
      _pos(0),
      _size(0),
      _filePos(0) {

    _buf = (uint8_t*)heap_caps_malloc(BUF_SIZE, MALLOC_CAP_DMA);
    if (!_buf) {
        _buf = (uint8_t*)malloc(BUF_SIZE);
    }
}

SdReadBuffer::~SdReadBuffer() {
    if (_buf) {
        free(_buf);
        _buf = nullptr;
    }
}

bool SdReadBuffer::available() const {
    if (_pos < _size) return true;
    return _file && _file->available();
}

void SdReadBuffer::refill() {
    if (!_file || !_buf) return;

    _size = _file->read(_buf, BUF_SIZE);
    _pos = 0;
    _filePos += _size;
}

uint8_t SdReadBuffer::readU8() {
    if (_pos >= _size) {
        refill();
        if (_size == 0) return 0;
    }
    return _buf[_pos++];
}

uint16_t SdReadBuffer::readU16() {
    uint16_t v = readU8();
    v |= (uint16_t)readU8() << 8;
    return v;
}

size_t SdReadBuffer::readBytes(void* dst, size_t len) {
    uint8_t* out = (uint8_t*)dst;
    size_t total = 0;

    while (len > 0) {
        size_t avail = _size - _pos;

        if (avail == 0) {
            refill();
            avail = _size;
            if (avail == 0) break;
        }

        size_t chunk = (len < avail) ? len : avail;
        memcpy(out, _buf + _pos, chunk);

        _pos += chunk;
        out += chunk;
        len -= chunk;
        total += chunk;
    }

    return total;
}

bool SdReadBuffer::seek(size_t pos) {
    if (!_file) return false;

    // позиционируем файл
    if (!_file->seek(pos)) {
        return false;
    }

    // сбрасываем буфер
    _pos = 0;
    _size = 0;
    _filePos = pos;

    return true;
}

size_t SdReadBuffer::tell() const {
    return _filePos - (_size - _pos);
}
