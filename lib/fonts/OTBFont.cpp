#include "OTBFont.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TAG(a,b,c,d) ((a<<24)|(b<<16)|(c<<8)|(d))

// --------------------
// Helpers
// --------------------

static uint8_t readU8(File& f) {
    uint8_t b;
    f.read(&b,1);
    return b;
}

static uint16_t readU16(File& f) {
    uint8_t b[2];
    f.read(b,2);
    return (b[0]<<8)|b[1];
}

static uint32_t readU32(File& f) {
    uint8_t b[4];
    f.read(b,4);
    return (b[0]<<24)|(b[1]<<16)|(b[2]<<8)|b[3];
}

// --------------------
// Constructor / Destructor
// --------------------

OTBFont::OTBFont() :
    _bitmapBlock(nullptr),
    _bitmapBlockSize(0),
    _firstGlyph(0),
    _lastGlyph(0),
    _glyphSize(0),
    _fontWidth(0),
    _fontHeight(0)
{
}

OTBFont::~OTBFont() {
    unload();
}

void OTBFont::unload() {
    if(_bitmapBlock)
    {
        free(_bitmapBlock);
        _bitmapBlock = nullptr;
    }

    _bitmapBlockSize = 0;
    _glyphSize = 0;
}

// --------------------
// Public API
// --------------------

bool OTBFont::load(File& f) {
    unload();

    if(!f) {
        return false;
    }

    uint32_t eblcOffset = 0;
    uint32_t ebdtOffset = 0;

    if(!parseSFNT(f, eblcOffset, ebdtOffset)) {
        return false;
    }

    uint32_t subTableOffset = 0;
    uint16_t firstGlyph = 0;
    uint16_t lastGlyph  = 0;

    if(!parseEBLC(f, eblcOffset, subTableOffset, firstGlyph, lastGlyph)) {
        return false;
    }

    if(!loadBitmaps(f, ebdtOffset, subTableOffset, firstGlyph, lastGlyph)) {
        return false;
    }

    return true;
}

const uint8_t* OTBFont::getBitmap(uint8_t ascii) const {
    if(!_bitmapBlock)
        return nullptr;

    if(ascii < _firstGlyph || ascii > _lastGlyph)
        return nullptr;

    uint32_t index = ascii - _firstGlyph + 1;
    return _bitmapBlock + index * _glyphSize;
}

// --------------------
// Parsing
// --------------------

bool OTBFont::parseSFNT(File& f, uint32_t& eblcOffset, uint32_t& ebdtOffset) {
    f.seek(0, SeekSet);

    readU32(f); // sfnt version
    uint16_t numTables = readU16(f);
    f.seek(f.position() + 6, SeekSet);

    for(uint16_t i=0;i<numTables;i++) {
        uint32_t tag = readU32(f);
        readU32(f); // checksum
        uint32_t offset = readU32(f);
        readU32(f); // length

        if(tag == TAG('E','B','L','C'))
            eblcOffset = offset;

        if(tag == TAG('E','B','D','T'))
            ebdtOffset = offset;
    }

    return (eblcOffset && ebdtOffset);
}

bool OTBFont::parseEBLC(File& f, uint32_t eblcOffset, uint32_t& subTableOffset, uint16_t& firstGlyph, uint16_t& lastGlyph) {
    f.seek(eblcOffset, SeekSet);

    readU32(f); // version
    uint32_t numSizes = readU32(f);
    if(numSizes == 0) {
        return false;
    }

    uint32_t indexSubTableArrayOffset = readU32(f);
    readU32(f); // indexTablesSize
    readU32(f); // numberOfIndexSubTables
    readU32(f); // colorRef

    readU16(f); // startGlyphIndex (ignored)
    readU16(f); // endGlyphIndex   (ignored)

    f.seek(f.position() + 28, SeekSet); // skip metrics

    uint32_t arrayPos = eblcOffset + indexSubTableArrayOffset;
    f.seek(arrayPos, SeekSet);

    firstGlyph = readU16(f);
    lastGlyph  = readU16(f);
    uint32_t additionalOffset = readU32(f);

    subTableOffset = arrayPos + additionalOffset;

    return true;
}

// --------------------
// Bitmap loading (Format 2 only)
// --------------------

bool OTBFont::loadBitmaps(File& f, uint32_t ebdtOffset, uint32_t subTableOffset, uint16_t firstGlyph, uint16_t lastGlyph) {
    f.seek(subTableOffset, SeekSet);

    uint16_t indexFormat = readU16(f);
    uint16_t imageFormat = readU16(f);
    uint32_t imageDataOffset = readU32(f);

    if(indexFormat != 2) {
        return false;
    }

    uint32_t imageSize = readU32(f);

    _fontHeight = readU8(f);
    _fontWidth  = readU8(f);

    if(_fontWidth > 8) {
        return false;
    }

    // skip remaining BigGlyphMetrics
    f.seek(f.position() + 6, SeekSet);

    _firstGlyph = firstGlyph;
    _lastGlyph  = lastGlyph;
    _glyphSize  = imageSize;

    uint16_t glyphCount = lastGlyph - firstGlyph + 1;

    uint32_t totalSize = glyphCount * imageSize;

    _bitmapBlock = (uint8_t*)malloc(totalSize);
    if(!_bitmapBlock) {
        return false;
    }

    _bitmapBlockSize = totalSize;

    uint32_t bitmapStart = ebdtOffset + imageDataOffset;

    f.seek(bitmapStart, SeekSet);

    if(f.read(_bitmapBlock, totalSize) != totalSize) {
        return false;
    }

    return true;
}

// --------------------
// Debug
// --------------------

bool OTBFont::debugPrintGlyph(uint8_t ascii, char* buffer, size_t bufferSize) const {
    const uint8_t* bmp = getBitmap(ascii);
    if(!bmp || !buffer) {
        return false;
    }

    size_t pos = 0;

    int written = snprintf(buffer,
                           bufferSize,
                           "Glyph %d (%c): %dx%d\n",
                           ascii,
                           (ascii >= 32 && ascii < 127) ? ascii : '?',
                           _fontWidth,
                           _fontHeight);

    if(written <= 0)
        return false;

    pos = written;

    for(uint8_t y = 0; y < _fontHeight; y++) {
        uint8_t row = bmp[y];

        for(uint8_t x = 0; x < _fontWidth; x++) {
            if(pos >= bufferSize-2)
                return false;

            buffer[pos++] =
                (row & (0x80 >> x)) ? '#' : '.';
        }

        buffer[pos++] = '\n';
    }

    buffer[pos] = 0;
    return true;
}