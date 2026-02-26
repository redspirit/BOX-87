#pragma once
#include <stdint.h>
#include "TextTiles.h"
#include "SpritesRender.h"

#define MAX_IMAGES   4

struct InlineImage {
    int spriteIndex;
    int yPixels;
    int heightPixels;
    bool active;
};

class Console {
    public:
        Console();
        ~Console();

        void init(uint8_t defaultColor);

        TextTiles& tiles() { return tiles_; }
        SpritesRender& sprites() { return sprites_; };

        void clear();

        void setColor(uint8_t colorIndex);
        void setColorRaw(uint8_t color);
        void useDefaultColor();

        void print(const char* text);
        void printLn(const char* text);
        void printInt(int value);
        void printLn();

        void printRawChar(uint16_t c, uint16_t repeat = 1);

        void clearCharAt(int x, int y);

        void cursorSetup(char cursorChar, float cursorBlinkSpeed);
        void setCursorVisible(bool visible);
        bool const getCursorVisible();
        void setCursor(int x, int y);
        void getCursor(int& x, int& y) const;

        void cursorUpdate(float dt);

        void show();
        void show(int y1, int y2);
        int insertImage(uint8_t* buffer, int w, int h);

    private:
        // owned
        TextTiles tiles_;
        SpritesRender sprites_;
        InlineImage inlineImages_[MAX_IMAGES];

        int width_  = 0;
        int height_ = 0;

        uint8_t currentColor_ = 255; // white
        uint8_t defaultColor_ = 255; // white

        // ring buffer
        CharTile* buffer_ = nullptr;
        int head_  = 0;
        int count_ = 0;

        // cursor
        int cx_ = 0;
        int cy_ = 0;

        char cursorChar_ = '_';
        float blinkSpeed_ = 0.5f;
        float blinkTimer_ = 0.0f;
        bool cursorEnabled_ = false;   // управляется setCursorVisible()
        bool cursorPhase_   = true;    // мигание

    private:
        inline CharTile& cell(int x, int y);
        void newLine();
        void scrollUp();
        void clearLine(int row);
};
