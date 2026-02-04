#pragma once

#include <stdint.h>
#include <stddef.h>
#include <FS.h>
#include "esp_heap_caps.h" // Нужно для управления памятью

class SdReadBuffer {
public:
    explicit SdReadBuffer(fs::File* file);
    ~SdReadBuffer(); // Обязательно нужен деструктор для освобождения памяти

    uint8_t  readU8();
    uint16_t readU16();
    // Возвращает количество реально прочитанных байт
    size_t readBytes(void* dst, size_t len); 
    
    bool available() const;

private:
    fs::File* _file;

    // Указатель на буфер вместо статического массива
    uint8_t* _buf; 
    
    // Размер буфера (16KB - отлично, можно даже 32KB если RAM позволяет)
    static constexpr size_t BUF_SIZE = 16 * 1024; 

    size_t _pos = 0;
    size_t _size = 0; // Сколько байт реально сейчас в буфере

    void refill();
};