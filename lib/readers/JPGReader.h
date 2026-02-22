#pragma once
#include <stdint.h>
#include <SD.h>

extern "C" {
    #include "tjpgd.h"
}

class JPGReader
{
public:
    JPGReader();
    ~JPGReader();

    bool readHeader(File& file, int& w, int& h);

    bool read(File& file,
              uint8_t* dst,
              size_t dstSize);

private:
    File*    file_;
    uint8_t* dst_;
    int      width_;
    int      height_;

    uint8_t  workspace_[4096];

    static size_t inputFunc(JDEC* jd,
                            uint8_t* buff,
                            size_t nbyte);

    static int outputFunc(JDEC* jd,
                          void* bitmap,
                          JRECT* rect);

    static uint8_t rgb888_to_b2g3r3(uint8_t R,
                                    uint8_t G,
                                    uint8_t B);
};