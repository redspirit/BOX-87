#pragma once
#include <stdint.h>
#include "CharTile.h"

class VGA;
class TextTiles;

class Console {
    public:
        Console();
        ~Console();

        void init(
            VGA& vga,
            int tileW,
            int tileH,
            uint8_t defaultColor
        );

        // доступ к текстовому слою (если нужен снаружи)
        TextTiles& tiles();

        void clear();

        void setColor(uint8_t colorIndex);
        void setColorRaw(uint8_t color);
        void useDefaultColor();

        void print(const char* text);
        void print(char c);
        void print(int value);

        void printLn();
        void printLn(const char* text);
        void printLn(int value);

        void printRawChar(char c, uint16_t repeat = 1);

        void clearCharAt(int x, int y);

        void cursorSetup(char cursorChar, float cursorBlinkSpeed);
        void setCursorVisible(bool visible);
        void setCursor(int x, int y);
        void getCursor(int& x, int& y) const;

        void cursorUpdate(float dt);

        void show();
        void show(int y1, int y2);
        void insertLogo(const uint8_t* data, uint16_t w, uint16_t h, int16_t x, int16_t y);

    private:
        // owned
        TextTiles* tiles_ = nullptr;
        VGA* vga_ = nullptr;

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

        int16_t _logoY = 0;

    private:
        inline CharTile& cell(int x, int y);
        void newLine();
        void scrollUp();
        void clearLine(int row);
};
