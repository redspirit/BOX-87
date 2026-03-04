#pragma once
#include "ISubsystem.h"
#include "TextTiles.h"
#include "SdReadBuffer.h"
#include "Audio.h"

#define MAX_PATH 32

class Player : public ISubsystem {
    public:
        Player(const char* fullPath);
        ~Player();

        bool init() override;
        void update(float dt) override;
        void tick() override;

        uint32_t frameTimeMs() const override {
            return _frameTimeMs;
        }

    private:
        TextTiles _tiles;
        SdReadBuffer* _rb = nullptr;
        Audio _audio;

        char _path[MAX_PATH];
        
        uint32_t _frameTimeMs = 16;
        bool open(const char* path);
        void playFrame();
        bool isFinished() const;
        void doPause();

        // RV header
        uint16_t width;
        uint16_t height;
        uint16_t fps;
        uint32_t frameCount;
        uint8_t  bpp;
        uint8_t  tileW;
        uint8_t  tileH;
        uint32_t videoOffset;
        uint32_t audioOffset;
        uint32_t audioSamples;
        uint32_t audioRate;

        uint16_t tilesX;
        uint16_t tilesY;

        uint32_t currentFrame;
        uint32_t _vgaYOffset;

        bool _isPause;

        uint8_t* stateFB;

        // helpers
        bool readHeader();
        void unpackTile(uint16_t tileIndex, const uint8_t* data);
};