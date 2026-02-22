#pragma once
#include <stdint.h>
#include <SD.h>

class PNGReader {
    public:
        PNGReader();
        ~PNGReader();

        bool readHeader(File& file, int& w, int& h);

        bool read(File& file,
                uint8_t* dst,
                size_t dstSize);

    private:
        uint8_t* fileBuffer_;
        size_t   fileSize_;

        uint8_t* decodeBuffer_;
        size_t   decodeSize_;

        uint8_t rgb888_to_b2g3r3(uint8_t R,
                                uint8_t G,
                                uint8_t B);

        bool loadFileToPSRAM(File& file);
        void freeBuffers();
    };