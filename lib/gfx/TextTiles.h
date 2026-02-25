#pragma once
#include <stdint.h>
#include "CharTile.h"
#include "OTBFont.h"

class TextTiles {
public:
    using Tile = CharTile;

    TextTiles() = default;
    ~TextTiles();

    void init();

    void clear();

    bool setFontFile(const char* path, bool isSD);

    void drawTile(int x, int y, CharTile t);
    void drawTileForeground(int x, int y, CharTile t);
    void foregroundVisible(bool visible);

    void print(const char* text, int x, int y, uint8_t color);
    void render();
    void renderTileBitmap(int px, int py, const uint8_t* glyph);

    int width()  const { return gridW_; }
    int height() const { return gridH_; }

    void setTransparent(bool enabled);
    OTBFont& getFont();

private:
    OTBFont _font;
    int gridW_ = 0;
    int gridH_ = 0;
    int tileW_ = 0;
    int tileH_ = 0;

    CharTile* tilemap_ = nullptr;

    // foreground
    CharTile fgTile_{};
    int fgX_ = 0;
    int fgY_ = 0;
    bool fgVisible_ = false;
    
    bool transparent_ = true;

    inline CharTile& tileAt(int x, int y) {
        return tilemap_[y * gridW_ + x];
    }

    void renderTile(int px, int py, const CharTile& t);
    void drawText();
    void drawCursor();
};
