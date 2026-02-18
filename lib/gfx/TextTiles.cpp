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

    tileW_ = _font.getWidth();;
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

void TextTiles::setImage(
    const uint8_t* data,
    uint16_t w,
    uint16_t h,
    int16_t x,
    int16_t y
) {
    _image.data = data;
    _image.width = w;
    _image.height = h;
    _image.x = x;
    _image.y = y;
    _image.enabled = true;
}

void TextTiles::hideImage() {
    _image.enabled = false;
}

void TextTiles::imageY(int16_t y) {
    _image.y = y;

    if (!_image.data || !_image.height) {
        _image.enabled = false;
        return;
    }

    const int screenH = VGA::height();

    // положение картинки на экране
    int screenTop    = _image.y;
    int screenBottom = screenTop + _image.height;

    // проверка пересечения с экраном
    if (screenBottom <= 0 || screenTop >= screenH) {
        _image.enabled = false;
    } else {
        _image.enabled = true;
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

void TextTiles::drawImage() {
    if (!_image.enabled || !_image.data)
        return;

    const int screenW = VGA::width();
    const int screenH = VGA::height();

    const int bytesPerRow = _image.width / 4;

    for (int y = 0; y < _image.height; y++) {
        int sy = _image.y + y;
        if (sy < 0 || sy >= screenH)
            continue;

        uint8_t* dst = VGA::getLinePtr8(sy);

        const uint8_t* src =
            _image.data + y * bytesPerRow;

        for (int bx = 0; bx < bytesPerRow; bx++) {
            uint8_t b = src[bx];

            int px = _image.x + bx * 4;
            if (px >= screenW || px + 3 < 0)
                continue;

            // распаковка 4 пикселей
            for (int i = 0; i < 4; i++) {
                int sx = px + i;
                if (sx < 0 || sx >= screenW)
                    continue;

                uint8_t colorIdx = (b >> (6 - i * 2)) & 0x03;
                if (colorIdx == 1) continue;

                dst[sx] = getColorByPalette(colorIdx + 96); // 96 смещение в палитре для цветов логотипа
            }
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
    drawImage();
    drawCursor();
}

void TextTiles::renderTile(int px, int py, const CharTile& t) {
    // const uint8_t* glyph = font8x8::get(t.ch);
    const uint8_t* glyph =_font.getBitmap(t.ch);

    for (int y = 0; y < tileH_; y++) {
        uint8_t row = glyph[y];
        uint8_t* dst = VGA::getLinePtr8(py + y) + px;

        for (int x = 0; x < tileW_; x++) {
            bool bit = row & (1 << (7 - x));

            if (bit) {
                dst[x] = t.color;
            } else if (!transparent_) {
                dst[x] = 0; // black
            }
            // если transparent_ == true и bit == 0 → ничего не делаем
        }
    }
}

void TextTiles::setTransparent(bool enabled) {
    transparent_ = enabled;
}