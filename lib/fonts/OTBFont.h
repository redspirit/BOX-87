#pragma once
#include <stdint.h>
#include <FS.h>

class OTBFont
{
public:
    OTBFont();
    ~OTBFont();

    bool load(File& f);
    void unload();

    const uint8_t* getBitmap(uint8_t ascii) const;

    uint8_t getWidth()  const { return _fontWidth; }
    uint8_t getHeight() const { return _fontHeight; }

    bool debugPrintGlyph(uint8_t ascii,
                         char* buffer,
                         size_t bufferSize) const;

private:
    bool parseSFNT(File& f,
                   uint32_t& eblcOffset,
                   uint32_t& ebdtOffset);

    bool parseEBLC(File& f,
                   uint32_t eblcOffset,
                   uint32_t& subTableOffset,
                   uint16_t& firstGlyph,
                   uint16_t& lastGlyph);

    bool loadBitmaps(File& f,
                     uint32_t ebdtOffset,
                     uint32_t subTableOffset,
                     uint16_t firstGlyph,
                     uint16_t lastGlyph);

private:
    uint8_t*  _bitmapBlock;
    uint32_t  _bitmapBlockSize;

    uint16_t  _firstGlyph;
    uint16_t  _lastGlyph;

    uint16_t  _glyphSize;     // размер одного glyph в байтах
    uint8_t   _fontWidth;
    uint8_t   _fontHeight;
};