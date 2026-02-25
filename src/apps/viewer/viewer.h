#pragma once
#include "ISubsystem.h"
#include "TextTiles.h"

#define MAX_PATH 128

class Viewer : public ISubsystem {
    public:
        Viewer(const char* fullPath);
        ~Viewer();

        bool init() override;
        void update(float dt) override;
        void tick() override;

        uint32_t frameTimeMs() const override {
            return _frameTimeMs;
        }

    private:
        TextTiles _tiles;

        char _path[MAX_PATH];

        uint32_t _frameTimeMs = 16;
        bool open(const char* path);
        bool hasExtension(const char* path, const char* ext);

};