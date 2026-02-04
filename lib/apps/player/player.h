#pragma once
#include "ISubsystem.h"
#include "SDCard.h"
#include "VGA.h"
#include "TextTiles.h"
#include "SdReadBuffer.h"
#include "shell/shell_parser.h"

#define MAX_PATH 128

class Player : public ISubsystem {
    public:
        Player(VGA& _vga, const ShellParser& args, const char* fullPath);
        ~Player();

        bool init() override;
        void update(float dt) override;
        void tick() override;

        uint32_t frameTimeMs() const override {
            return _frameTimeMs;
        }

    private:
        VGA& _vga;
        SDCard _sd;
        TextTiles _tiles;
        SdReadBuffer* _rb = nullptr;

        int  _argc;
        char _argv[SHELL_MAX_ARGS][SHELL_ARG_LEN];
        char _path[MAX_PATH];

        uint32_t _frameTimeMs = 16;
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