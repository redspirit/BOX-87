#pragma once
#include <stdint.h>

class SpritesRender {
public:
    struct Sprite {
        uint8_t* buffer;   // raw pixel data
        int x;
        int y;
        int w;
        int h;
        bool active;
    };

public:
    SpritesRender();
    ~SpritesRender();

    int  addSprite(uint8_t* buffer, int x, int y, int w, int h);
    void setPosition(int index, int x, int y);
    void setPositionY(int index, int y);
    void removeSprite(int index);

    void render();
    void clear();

private:
    static constexpr int MAX_SPRITES = 8;

    Sprite sprites_[MAX_SPRITES];

    void drawSprite(const Sprite& s);
};