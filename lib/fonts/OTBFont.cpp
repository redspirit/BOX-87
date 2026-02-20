#include "OTBFont.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <LOG.h>

#define TAG(a,b,c,d) ((a<<24)|(b<<16)|(c<<8)|(d))

static uint8_t readU8(File& f) {
    uint8_t b;
    f.read(&b, 1);
    return b;
}

static uint16_t readU16(File& f) {
    uint8_t b[2];
    f.read(b, 2);
    return (b[0] << 8) | b[1];
}

static uint32_t readU32(File& f) {
    uint8_t b[4];
    f.read(b, 4);
    return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}

// --------------------
// Constructor / Destructor
// --------------------

OTBFont::OTBFont() :
    _subTables(nullptr),
    _numSubTables(0),
    _fontWidth(0),
    _fontHeight(0)
{
    memset(&_cmap, 0, sizeof(CmapFormat4));
}

OTBFont::~OTBFont() {
    unload();
}

void OTBFont::freeCmap() {
    if (_cmap.endCode) free(_cmap.endCode);
    if (_cmap.startCode) free(_cmap.startCode);
    if (_cmap.idDelta) free(_cmap.idDelta);
    if (_cmap.idRangeOffset) free(_cmap.idRangeOffset);
    if (_cmap.glyphIdArray) free(_cmap.glyphIdArray);
    memset(&_cmap, 0, sizeof(CmapFormat4));
}

void OTBFont::unload() {
    // Освобождаем память всех субтаблиц
    if (_subTables) {
        for (uint32_t i = 0; i < _numSubTables; i++) {
            if (_subTables[i].data) {
                free(_subTables[i].data);
            }
        }
        free(_subTables);
        _subTables = nullptr;
    }
    _numSubTables = 0;
    _fontWidth = 0;
    _fontHeight = 0;

    freeCmap(); // Устраняем утечку памяти CMAP!
}

// --------------------
// Public API
// --------------------

bool OTBFont::load(File& f) {
    unload(); // Всегда очищаем перед новой загрузкой

    if (!f) return false;

    uint32_t eblcOffset = 0, ebdtOffset = 0, cmapOffset = 0;

    if (!parseSFNT(f, eblcOffset, ebdtOffset, cmapOffset)) {
        LOG.println("Error parsing SFNT offsets");
        return false;
    }

    if (!parseEBLC_EBDT(f, eblcOffset, ebdtOffset)) {
        LOG.println("Error parsing EBLC/EBDT");
        return false;
    }

    if (!parseCMAP(f, cmapOffset)) {
        LOG.println("Error parsing CMAP");
        return false;
    }

    return true;
}

// --------------------
// Parsing
// --------------------

bool OTBFont::parseSFNT(File& f, uint32_t& eblcOffset, uint32_t& ebdtOffset, uint32_t& cmapOffset) {
    readU32(f); // sfnt version
    uint16_t numTables = readU16(f);
    f.seek(f.position() + 6, SeekSet);

    for (uint16_t i = 0; i < numTables; i++) {
        uint32_t tag = readU32(f);
        readU32(f); // checksum
        uint32_t offset = readU32(f);
        readU32(f); // length

        if (tag == TAG('E','B','L','C')) eblcOffset = offset;
        if (tag == TAG('E','B','D','T')) ebdtOffset = offset;
        if (tag == TAG('c','m','a','p')) cmapOffset = offset;
    }

    return (eblcOffset && ebdtOffset && cmapOffset);
}

// Объединили разбор индексов и загрузку битмапов
bool OTBFont::parseEBLC_EBDT(File& f, uint32_t eblcOffset, uint32_t ebdtOffset) {
    f.seek(eblcOffset, SeekSet);

    readU32(f); // version
    uint32_t numSizes = readU32(f);
    if (numSizes == 0) return false;

    uint32_t indexSubTableArrayOffset = readU32(f);
    readU32(f); // indexTablesSize
    uint32_t numberOfIndexSubTables = readU32(f);
    readU32(f); // colorRef

    // Пропускаем остатки bitmapSizeTable (смещения метрик)
    // startGlyphIndex (2) + endGlyphIndex (2) + ppemX/Y/flags (4) + metrics (24) = 32 байта
    // Но безопаснее просто прыгнуть на смещение массива
    uint32_t arrayPos = eblcOffset + indexSubTableArrayOffset;
    
    _numSubTables = numberOfIndexSubTables;
    _subTables = (SubTable*)malloc(sizeof(SubTable) * _numSubTables);
    if (!_subTables) return false;
    memset(_subTables, 0, sizeof(SubTable) * _numSubTables);

    // Читаем заголовки всех субтаблиц (где лежат, какие глифы покрывают)
    struct SubTableInfo {
        uint16_t firstGlyph;
        uint16_t lastGlyph;
        uint32_t offset;
    };
    
    SubTableInfo* infos = (SubTableInfo*)malloc(sizeof(SubTableInfo) * _numSubTables);
    if (!infos) return false;

    f.seek(arrayPos, SeekSet);
    for (uint32_t i = 0; i < _numSubTables; i++) {
        infos[i].firstGlyph = readU16(f);
        infos[i].lastGlyph  = readU16(f);
        infos[i].offset     = readU32(f); // Смещение от arrayPos
    }

    // Теперь читаем данные для каждой субтаблицы
    bool success = true;
    for (uint32_t i = 0; i < _numSubTables; i++) {
        uint32_t subTableOffset = arrayPos + infos[i].offset;
        f.seek(subTableOffset, SeekSet);

        uint16_t indexFormat = readU16(f);
        uint16_t imageFormat = readU16(f);
        uint32_t imageDataOffset = readU32(f);

        // OTB обычно используют format 2 (все битмапы одного размера)
        if (indexFormat != 2) {
            // LOG.printf("Warning: Unsupported indexFormat %d\n", indexFormat);
            continue;
        }

        uint32_t imageSize = readU32(f);
        _fontHeight = readU8(f); // Размеры считаем едиными для всего шрифта
        _fontWidth  = readU8(f);

        _subTables[i].firstGlyph = infos[i].firstGlyph;
        _subTables[i].lastGlyph  = infos[i].lastGlyph;
        _subTables[i].imageSize  = imageSize;

        uint32_t glyphCount = _subTables[i].lastGlyph - _subTables[i].firstGlyph + 1;
        uint32_t dataSize = glyphCount * imageSize;

        _subTables[i].data = (uint8_t*)malloc(dataSize);
        if (!_subTables[i].data) {
            success = false; break;
        }

        // Прыгаем в EBDT и читаем все растры этой субтаблицы в память
        uint32_t bitmapStart = ebdtOffset + imageDataOffset;
        f.seek(bitmapStart, SeekSet);
        if (f.read(_subTables[i].data, dataSize) != dataSize) {
            success = false; break;
        }
    }

    free(infos);
    return success;
}

bool OTBFont::parseCMAP(File& f, uint32_t cmapOffset) {
    f.seek(cmapOffset, SeekSet);

    readU16(f); // version
    uint16_t numTables = readU16(f);

    uint32_t unicodeOffset = 0;

    for (int i = 0; i < numTables; i++) {
        uint16_t platformID = readU16(f);
        uint16_t encodingID = readU16(f);
        uint32_t offset     = readU32(f);

        // Ищем Unicode кодировку (Windows Unicode BMP или Unicode 2.0)
        if ((platformID == 3 && encodingID == 1) || 
            (platformID == 0 && (encodingID == 3 || encodingID == 4))) {
            unicodeOffset = cmapOffset + offset;
        }
    }

    if (!unicodeOffset) return false;

    f.seek(unicodeOffset, SeekSet);

    uint16_t format = readU16(f);
    if (format != 4) return false;

    uint16_t length = readU16(f);
    readU16(f); // language

    uint16_t segCountX2 = readU16(f);
    uint16_t segCount   = segCountX2 / 2;

    f.seek(f.position() + 6, SeekSet); // Пропуск searchRange, entrySelector, rangeShift

    _cmap.segCount = segCount;

    _cmap.endCode = (uint16_t*)malloc(segCount * sizeof(uint16_t));
    for (int i = 0; i < segCount; i++) _cmap.endCode[i] = readU16(f);

    readU16(f); // reservedPad

    _cmap.startCode = (uint16_t*)malloc(segCount * sizeof(uint16_t));
    for (int i = 0; i < segCount; i++) _cmap.startCode[i] = readU16(f);

    _cmap.idDelta = (int16_t*)malloc(segCount * sizeof(int16_t));
    for (int i = 0; i < segCount; i++) _cmap.idDelta[i] = (int16_t)readU16(f);

    _cmap.idRangeOffset = (uint16_t*)malloc(segCount * sizeof(uint16_t));
    for (int i = 0; i < segCount; i++) _cmap.idRangeOffset[i] = readU16(f);

    uint32_t glyphArrayBytes = length - (f.position() - unicodeOffset);
    _cmap.glyphArrayCount = glyphArrayBytes / 2;
    _cmap.glyphIdArray = (uint16_t*)malloc(glyphArrayBytes);

    for (uint32_t i = 0; i < _cmap.glyphArrayCount; i++) {
        _cmap.glyphIdArray[i] = readU16(f);
    }

    return true;
}

uint16_t OTBFont::unicodeToGlyph(uint16_t code) const {
    for (uint16_t i = 0; i < _cmap.segCount; i++) {
        if (code < _cmap.startCode[i] || code > _cmap.endCode[i]) continue;

        if (_cmap.idRangeOffset[i] == 0) {
            return (code + _cmap.idDelta[i]) & 0xFFFF;
        } else {
            uint16_t offset = _cmap.idRangeOffset[i] / 2;
            uint16_t index = offset + (code - _cmap.startCode[i]) - (_cmap.segCount - i);

            if (index >= _cmap.glyphArrayCount) return 0;

            uint16_t glyph = _cmap.glyphIdArray[index];
            if (glyph != 0) {
                glyph = (glyph + _cmap.idDelta[i]) & 0xFFFF;
            }
            return glyph;
        }
    }
    return 0; // Не найдено
}

const uint8_t* OTBFont::getBitmapUnicode(uint16_t code) const {
    uint16_t glyph = unicodeToGlyph(code);
    if (glyph == 0) return nullptr;

    // Быстрый поиск нужной субтаблицы
    for (uint32_t i = 0; i < _numSubTables; i++) {
        if (glyph >= _subTables[i].firstGlyph && glyph <= _subTables[i].lastGlyph) {
            uint32_t index = glyph - _subTables[i].firstGlyph;
            return _subTables[i].data + (index * _subTables[i].imageSize);
        }
    }

    return nullptr;
}

// --------------------
// Debug
// --------------------

bool OTBFont::debugPrintGlyph(uint16_t code, char* buffer, size_t bufferSize) const {
    LOG.print("debugPrintGlyph code: ");
    LOG.println(code);
    const uint8_t* bmp = getBitmapUnicode(code);
    if (!bmp || !buffer) return false;

    size_t pos = 0;
    int written = snprintf(buffer, bufferSize, 
                           "Glyph %d (%c): %dx%d\n", 
                           code, (code >= 32 && code < 127) ? code : '?', 
                           _fontWidth, _fontHeight);

    if (written <= 0) return false;
    pos = written;

    for (uint8_t y = 0; y < _fontHeight; y++) {
        uint8_t row = bmp[y];

        for (uint8_t x = 0; x < _fontWidth; x++) {
            if (pos >= bufferSize - 2) return false; // Защита буфера
            // Проверка старшего бита, сдвиг влево
            buffer[pos++] = (row & (0x80 >> x)) ? '#' : '.'; 
        }
        buffer[pos++] = '\n';
    }

    buffer[pos] = 0;
    return true;
}

void OTBFont::forEachUnicode(std::function<void(uint16_t code)> callback) const {
    if (!_cmap.startCode || !callback) return;

    for (uint16_t i = 0; i < _cmap.segCount; i++) {
        uint16_t start = _cmap.startCode[i];
        uint16_t end = _cmap.endCode[i];

        // 0xFFFF — это технический терминатор в cmap format 4, его пропускаем
        if (start == 0xFFFF) continue;

        for (uint32_t code = start; code <= end; code++) {
            // Мы вызываем callback только если для кода есть реальный глиф
            if (unicodeToGlyph(code) != 0) {
                callback((uint16_t)code);
            }
        }
    }
}