#include "TextTiles.h"
#include "VGA/VGA.h"
#include "palette.h"
#include <string.h>
#include <stdlib.h>
#include <LittleFS.h>
#include <LOG.h>

TextTiles::~TextTiles() {
    if (tilemap_) {
        free(tilemap_);
        tilemap_ = nullptr;
    }
}

void TextTiles::init() {
    if (!setFontFile("/fonts/ToshibaSat_8x16.otb", false)) {
    // if (!setFontFile("/fonts/IBM_EGA_8x8.otb", false)) {
        LOG.println("[TextTiles] init failed");
    }
}

bool TextTiles::setFontFile(const char* path, bool isSD) {
    if (isSD) {
        // load from SD Card


    } else {
        // load from littleFS

        if(!LittleFS.begin()) {
            LOG.println("[TextTiles] LittleFS mount failed");
            return false;
        }

        File f = LittleFS.open(path);
        if(!f) {
            LOG.println("[TextTiles] Failed to open font file");
            return false;
        }

        if(!_font.load(f)){
            LOG.println("[TextTiles] Font load error");
            return false;
        }

        f.close();
        LittleFS.end();
    }

    tileW_ = _font.getWidth();
    tileH_ = _font.getHeight();

    gridW_ = VGA::width() / tileW_; // округляется вниз до целого автоматически
    gridH_ = VGA::height() / tileH_;

    // переаллоцируем, если нужно
    size_t count = gridW_ * gridH_;

    if (tilemap_) {
        free(tilemap_);
        tilemap_ = nullptr;
    }

    tilemap_ = (CharTile*)malloc(count * sizeof(CharTile));
    memset(tilemap_, 0, count * sizeof(CharTile));

    fgVisible_ = false;

    return true;
}

void TextTiles::clear() {
    if (!tilemap_) return;
    memset(tilemap_, 0, gridW_ * gridH_ * sizeof(CharTile));
}

void TextTiles::drawTile(int x, int y, CharTile t) {
    if (x < 0 || y < 0 || x >= gridW_ || y >= gridH_)
        return;

    tileAt(x, y) = t;
}

void TextTiles::drawTileForeground(int x, int y, CharTile t) {
    fgX_ = x;
    fgY_ = y;
    t.isForeground = true;
    fgTile_ = t;
}

void TextTiles::foregroundVisible(bool visible) {
    fgVisible_ = visible;
}

void TextTiles::print(const char* text, int x, int y, uint8_t color) {
    if (y < 0 || y >= gridH_)
        return;

    int cx = x;

    while (*text && cx < gridW_) {
        CharTile& t = tileAt(cx, y);
        t.ch = (uint8_t)*text++;
        t.color = color;
        cx++;
    }
}


void TextTiles::drawText() {
    for (int ty = 0; ty < gridH_; ty++) {
        for (int tx = 0; tx < gridW_; tx++) {
            const CharTile& t = tileAt(tx, ty);
            if (t.ch == 0)
                continue;

            renderTile(tx * tileW_, ty * tileH_, t);
        }
    }
}

void TextTiles::drawCursor() {
    if (fgVisible_) {
        renderTile(
            fgX_ * tileW_,
            fgY_ * tileH_,
            fgTile_
        );
    }
}

void TextTiles::render() {
    if (!tilemap_)
        return;

    drawText();
    drawCursor();
}

void TextTiles::renderTile(int px, int py, const CharTile& t) {
    const uint8_t* glyph = _font.getBitmapUnicode(t.ch);

    if (!glyph) {
        // fallback символ
        glyph = _font.getBitmapUnicode('?');
        if (!glyph) return;
    }

    for (int y = 0; y < tileH_; y++) {
        uint8_t row = glyph[y];
        uint8_t* dst = VGA::getLinePtr8(py + y) + px;

        for (int x = 0; x < tileW_; x++) {
            bool bit = row & (1 << (7 - x));

            if (bit) {
                if (t.isForeground && t.isInversion) {
                    // Инвертируем пиксель фона XOR'ом с цветом курсора
                    dst[x] = dst[x] ^ t.color;
                } else {
                    dst[x] = t.color;
                }
            } else if (!transparent_) {
                dst[x] = 0; // black
            }
            // если transparent_ == true и bit == 0 → ничего не делаем
        }
    }
}

void TextTiles::renderTileBitmap(int px, int py, const uint8_t* glyph) {
    if (!glyph) {
        return; 
    }

    for (int y = 0; y < tileH_; y++) {
        uint8_t row = glyph[y];
        uint8_t* dst = VGA::getLinePtr8(py + y) + px;

        for (int x = 0; x < tileW_; x++) {
            bool bit = row & (1 << (7 - x));

            if (bit) {
                dst[x] = 255;
            } else {
                dst[x] = 0;
            }
        }
    }
}

void TextTiles::setTransparent(bool enabled) {
    transparent_ = enabled;
}

OTBFont& TextTiles::getFont() {
    return _font;
}