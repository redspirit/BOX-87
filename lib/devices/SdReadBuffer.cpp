#include "SdReadBuffer.h"
#include <string.h>

SdReadBuffer::SdReadBuffer(fs::File* file)
    : _file(file) {
}

bool SdReadBuffer::available() const {
    return _file && (_pos < _size || _file->available());
}

void SdReadBuffer::refill() {
    _size = _file->read(_buf, BUF_SIZE);
    _pos = 0;
}

void SdReadBuffer::ensure(size_t needed) {
    if (_pos + needed <= _size)
        return;

    refill();
}

uint8_t SdReadBuffer::readU8() {
    ensure(1);
    return _buf[_pos++];
}

uint16_t SdReadBuffer::readU16() {
    uint16_t v = readU8();
    v |= uint16_t(readU8()) << 8;
    return v;
}

void SdReadBuffer::readBytes(void* dst, size_t len) {
    uint8_t* out = (uint8_t*)dst;

    while (len) {
        ensure(1);

        size_t avail = _size - _pos;
        size_t chunk = (len < avail) ? len : avail;

        memcpy(out, _buf + _pos, chunk);

        _pos += chunk;
        out  += chunk;
        len  -= chunk;
    }
}
