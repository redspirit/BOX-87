#pragma once
#include <stdint.h>
#include <SD.h>

class BMPReader
{
public:
    BMPReader();
    ~BMPReader();

    bool readHeader(File& file, int& w, int& h);
    bool read(File& file, uint8_t* dst, size_t dstSize);

private:
    uint8_t* lineBuffer_;   // PSRAM
    size_t   lineBufferSize_;

    uint8_t  palette_[256]; // LUT в B2G3R3

    uint16_t read16(File& f);
    uint32_t read32(File& f);

    uint8_t rgb888_to_b2g3r3(uint8_t R,
                             uint8_t G,
                             uint8_t B);
};