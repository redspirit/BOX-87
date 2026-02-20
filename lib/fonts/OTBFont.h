#pragma once
#include <stdint.h>
#include <FS.h>

struct CmapFormat4 {
    uint16_t  segCount;
    uint16_t* endCode;
    uint16_t* startCode;
    int16_t* idDelta;
    uint16_t* idRangeOffset;
    uint16_t* glyphIdArray;
    uint16_t  glyphArrayCount;
};

// Структура для хранения пачки растров (субтаблицы)
struct SubTable {
    uint16_t firstGlyph;
    uint16_t lastGlyph;
    uint32_t imageSize;
    uint8_t* data;
};

class OTBFont {
public:
    OTBFont();
    ~OTBFont();

    bool load(File& f);
    void unload();

    // Быстрый доступ к растру по Unicode
    const uint8_t* getBitmapUnicode(uint16_t code) const;

    uint8_t getWidth()  const { return _fontWidth; }
    uint8_t getHeight() const { return _fontHeight; }

    bool debugPrintGlyph(uint16_t code, char* buffer, size_t bufferSize) const;
    void forEachUnicode(std::function<void(uint16_t code)> callback) const;

private:
    bool parseSFNT(File& f, uint32_t& eblcOffset, uint32_t& ebdtOffset, uint32_t& cmapOffset);
    bool parseEBLC_EBDT(File& f, uint32_t eblcOffset, uint32_t ebdtOffset);
    bool parseCMAP(File& f, uint32_t cmapOffset);
    
    uint16_t unicodeToGlyph(uint16_t code) const;
    void freeCmap();

private:
    SubTable* _subTables;
    uint32_t  _numSubTables;

    uint8_t   _fontWidth;
    uint8_t   _fontHeight;

    CmapFormat4 _cmap;
};