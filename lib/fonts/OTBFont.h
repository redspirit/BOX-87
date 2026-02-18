#pragma once
#include <stdint.h>
#include <stdio.h>
#include <FS.h>

class OTBFont {
    public:
        OTBFont();
        ~OTBFont();

        bool load(File& f);

        const uint8_t* getBitmap(uint8_t ascii) const;
        uint8_t getWidth() const;
        uint8_t getHeight() const;

        // buffer должен быть >= (height * (width+1) + 32)
        bool debugPrintGlyph(uint8_t ascii,
                            char* buffer,
                            size_t bufferSize) const;

    private:
        struct Glyph {
            uint8_t width;
            uint8_t height;
            uint16_t stride;
            uint8_t* bitmap;
        };

        Glyph _glyphs[256];

        uint8_t _fontWidth;
        uint8_t _fontHeight;

        bool parseSFNT(File& f, uint32_t& eblcOffset, uint32_t& ebdtOffset);
        bool parseEBLC(File& f, uint32_t eblcOffset, uint32_t& subTableOffset, uint16_t& startGlyph, uint16_t& endGlyph);
        bool loadBitmaps(File& f, uint32_t ebdtOffset, uint32_t subTableOffset, uint16_t startGlyph, uint16_t endGlyph);
};