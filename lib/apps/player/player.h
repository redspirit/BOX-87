#pragma once
#include "ISubsystem.h"
#include "SDCard.h"
#include "VGA.h"
#include "TextTiles.h"
#include "SdReadBuffer.h"

class Player : public ISubsystem {
    public:
        Player(VGA& _vga);
        ~Player();

        bool init() override;
        void update(float dt) override;
        void tick() override;

    private:
        VGA& _vga;
        SDCard _sd;
        TextTiles _tiles;
        SdReadBuffer* _rb = nullptr;

        bool open(const char* path);
        void playFrame();
        bool isFinished() const;

        // header
        uint16_t width;
        uint16_t height;
        uint8_t  tileW;
        uint8_t  tileH;
        uint8_t  bpp;
        uint8_t  fps;
        uint16_t frameCount;
        uint16_t paletteSize;
        uint16_t keyframeInterval;
        bool directColor;

        // playback
        uint16_t currentFrame = 0;
        uint8_t* stateFB; // width * height

        // geometry
        uint16_t tilesX;
        uint16_t tilesY;

        // palette
        uint8_t palette[256]; // формат 332

        // helpers
        bool readHeader();
        void readPalette();
        void unpackTile(uint16_t tileIndex, const uint8_t* data);
        void testSDReadSpeed(const char* path);
};