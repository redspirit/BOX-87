#include "OTBFont.h"
#include <stdlib.h>
#include <string.h>
#include <LOG.h>

#define TAG(a,b,c,d) ((a<<24)|(b<<16)|(c<<8)|(d))

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

OTBFont::OTBFont() {
    memset(_glyphs,0,sizeof(_glyphs));
}

OTBFont::~OTBFont() {
    for(int i=0;i<256;i++){
        if(_glyphs[i].bitmap)
            free(_glyphs[i].bitmap);
    }
}

bool OTBFont::load(File& f) {
    if(!f)
        return false;

    uint32_t eblcOffset = 0;
    uint32_t ebdtOffset = 0;

    if(!parseSFNT(f, eblcOffset, ebdtOffset))
        return false;

    LOG.println("parseSFNT - OK");

    uint32_t subTableOffset = 0;
    uint16_t startGlyph = 0;
    uint16_t endGlyph = 0;

    if(!parseEBLC(f,
                  eblcOffset,
                  subTableOffset,
                  startGlyph,
                  endGlyph))
        return false;

    LOG.println("parseEBLC - OK");

    if(!loadBitmaps(f,
                    ebdtOffset,
                    subTableOffset,
                    startGlyph,
                    endGlyph))
        return false;

    LOG.println("loadBitmaps - OK");

    return true;
}

bool OTBFont::parseSFNT(File& f, uint32_t& eblcOffset, uint32_t& ebdtOffset) {
    f.seek(0, SeekSet);

    readU32(f); // sfnt version
    uint16_t numTables = readU16(f);
    f.seek(f.position() + 6, SeekSet); // skip searchRange etc

    for(int i=0;i<numTables;i++){
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

bool OTBFont::parseEBLC(File& f,
                        uint32_t eblcOffset,
                        uint32_t& subTableOffset,
                        uint16_t& startGlyph,
                        uint16_t& endGlyph)
{
    f.seek(eblcOffset, SeekSet);

    readU32(f); // version
    uint32_t numSizes = readU32(f);

    if(numSizes == 0)
        return false;

    uint32_t indexSubTableArrayOffset = readU32(f);
    readU32(f); // indexTablesSize
    readU32(f); // numberOfIndexSubTables
    readU32(f); // colorRef

    readU16(f); // startGlyph
    readU16(f); // endGlyph

    readU8(f); // ppemX
    readU8(f); // ppemY
    readU8(f); // bitDepth
    readU8(f); // flags

    // skip 24 bytes SbitLineMetrics
    f.seek(f.position() + 24, SeekSet);

    // --- Читаем IndexSubTableArray ---
    uint32_t arrayPos = eblcOffset + indexSubTableArrayOffset;
    f.seek(arrayPos, SeekSet);

    startGlyph = readU16(f); // firstGlyphIndex
    endGlyph  = readU16(f); // lastGlyphIndex
    uint32_t additionalOffsetToIndexSubtable = readU32(f);

    subTableOffset = arrayPos + additionalOffsetToIndexSubtable;

    return true;
}

bool OTBFont::loadBitmaps(File& f,
                          uint32_t ebdtOffset,
                          uint32_t subTableOffset,
                          uint16_t startGlyph,
                          uint16_t endGlyph)
{

    f.seek(subTableOffset, SeekSet);

    uint16_t indexFormat = readU16(f);
    uint16_t imageFormat = readU16(f);
    uint32_t imageDataOffset = readU32(f);

    LOG.printf("indexFormat = %u\n", indexFormat);
    LOG.printf("imageFormat = %u\n", imageFormat);
    LOG.printf("imageDataOffset = %lu\n", imageDataOffset);

    if(indexFormat == 2) {
        uint32_t imageSize = readU32(f);

        // читаем BigGlyphMetrics (общие для всех)
        uint8_t height = readU8(f);
        uint8_t width  = readU8(f);

        LOG.println("loadBitmaps - height/width/imageSize"); 
        LOG.println(height); 
        LOG.println(width);
        LOG.println(imageSize);

        int8_t horiBearingX = (int8_t)readU8(f);
        int8_t horiBearingY = (int8_t)readU8(f);
        readU8(f); // horiAdvance
        readU8(f); // vertBearingX
        readU8(f); // vertBearingY
        readU8(f); // vertAdvance

        if(width > 8) {
            LOG.println("ERROR FONT LOAD - font width large than 8"); 
            return false;
        }
            
        uint16_t glyphCount = endGlyph - startGlyph + 1;

        LOG.printf("EBDT glyphCount = %lu\n", glyphCount);
        LOG.printf("EBDT offset = %lu\n", ebdtOffset);
        LOG.printf("First glyphOffset = %lu\n", ebdtOffset + 4 + imageDataOffset);
        LOG.printf("File size = %lu\n", f.size());

        for(uint16_t g = 0; g < glyphCount; g++) {
            uint8_t* buffer = (uint8_t*)malloc(imageSize);
            if(!buffer)
                return false;

            size_t readBytes = f.read(buffer, imageSize);

            if(readBytes != imageSize)
                return false;

            uint8_t ascii = startGlyph + g;

            _glyphs[ascii].width  = width;
            _glyphs[ascii].height = height;
            _glyphs[ascii].stride = (width + 7) >> 3;
            _glyphs[ascii].bitmap = buffer;

            _fontWidth  = width;
            _fontHeight = height;
        }

        return true;
    } else if (indexFormat == 1) {
        uint16_t glyphCount = endGlyph - startGlyph + 1;

        // читаем offsets[]
        uint32_t* offsets = (uint32_t*)malloc((glyphCount + 1) * sizeof(uint32_t));
        if(!offsets) return false;

        for(uint16_t i = 0; i <= glyphCount; i++)
            offsets[i] = readU32(f);

        for(uint16_t g = 0; g < glyphCount; g++)
        {
            uint32_t glyphOffset =
                ebdtOffset + imageDataOffset + offsets[g];

            f.seek(glyphOffset, SeekSet);

            uint8_t height = readU8(f);
            uint8_t width  = readU8(f);

            LOG.println("loadBitmaps - height/width"); 
            LOG.println(height); 
            LOG.println(width);

            if(width > 8)
            {
                free(offsets);
                return false;
            }

            readU8(f); // bearingX
            readU8(f); // bearingY
            readU8(f); // advance

            uint16_t bytesPerRow = (width + 7) >> 3;
            uint16_t bitmapSize = bytesPerRow * height;

            uint8_t* buffer = (uint8_t*)malloc(bitmapSize);
            if(!buffer)
            {
                free(offsets);
                return false;
            }

            if(f.read(buffer, bitmapSize) != bitmapSize)
            {
                free(offsets);
                return false;
            }

            uint8_t ascii = startGlyph + g;

            _glyphs[ascii].width  = width;
            _glyphs[ascii].height = height;
            _glyphs[ascii].stride = bytesPerRow;
            _glyphs[ascii].bitmap = buffer;

            _fontWidth  = width;
            _fontHeight = height;
        }

        free(offsets);
        return true;

    } else {
        LOG.print("Unsupported indexFormat - ");
        LOG.println(indexFormat);
        return false;
    }

}

const uint8_t* OTBFont::getBitmap(uint8_t ascii) const
{
    return _glyphs[ascii].bitmap;
}

uint8_t OTBFont::getWidth() const {
    return _fontWidth;
}

uint8_t OTBFont::getHeight() const {
    return _fontHeight;
}

bool OTBFont::debugPrintGlyph(uint8_t ascii, char* buffer, size_t bufferSize) const {
    const Glyph& g = _glyphs[ascii];

    if(!g.bitmap || !buffer)
        return false;

    size_t pos = 0;

    int written = snprintf(buffer + pos,
                           bufferSize - pos,
                           "Glyph %d (%c): %dx%d\n",
                           ascii,
                           (ascii >= 32 && ascii < 127) ? ascii : '?',
                           g.width,
                           g.height);

    if(written <= 0) return false;
    pos += written;

    for(uint8_t y = 0; y < g.height; y++)
    {
        uint8_t row = g.bitmap[y * g.stride];

        for(uint8_t x = 0; x < g.width; x++)
        {
            if(pos >= bufferSize - 2)
                return false;

            buffer[pos++] =
                (row & (0x80 >> x)) ? '#' : '.';
        }

        buffer[pos++] = '\n';
    }

    if(pos < bufferSize)
        buffer[pos] = 0;

    return true;
}