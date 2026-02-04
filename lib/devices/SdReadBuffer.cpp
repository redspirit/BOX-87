#include "SdReadBuffer.h"
#include <string.h>

SdReadBuffer::SdReadBuffer(fs::File* file) : _file(file), _pos(0), _size(0) {
    // Выделяем память строго в DMA-доступной SRAM (не PSRAM)
    _buf = (uint8_t*)heap_caps_malloc(BUF_SIZE, MALLOC_CAP_DMA);
    
    if (!_buf) {
        // Если памяти не хватило, можно попробовать обычный malloc или обработать ошибку
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
    // Читаем максимально возможный блок
    // SD_MMC любит большие блоки, кратные 512 байтам
    _size = _file->read(_buf, BUF_SIZE);
    _pos = 0;
}

uint8_t SdReadBuffer::readU8() {
    if (_pos >= _size) {
        refill();
        if (_size == 0) return 0; // EOF
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
    size_t totalRead = 0;

    while (len > 0) {
        size_t avail = _size - _pos;

        // Если в буфере пусто, заполняем
        if (avail == 0) {
            refill();
            avail = _size;
            // Если после refill всё еще 0 - значит конец файла
            if (avail == 0) break; 
        }

        // Берем сколько нужно или сколько есть
        size_t chunk = (len < avail) ? len : avail;

        memcpy(out, _buf + _pos, chunk);

        _pos += chunk;
        out += chunk;
        len -= chunk;
        totalRead += chunk;
    }
    
    return totalRead;
}