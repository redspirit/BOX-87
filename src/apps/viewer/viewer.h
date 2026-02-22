#pragma once
#include "ISubsystem.h"
#include "TextTiles.h"
#include "apps/shell/shell_parser.h"

#define MAX_PATH 128

class Viewer : public ISubsystem {
    public:
        Viewer(const ShellParser& args, const char* fullPath);
        ~Viewer();

        bool init() override;
        void update(float dt) override;
        void tick() override;

        uint32_t frameTimeMs() const override {
            return _frameTimeMs;
        }

    private:
        TextTiles _tiles;

        int  _argc;
        char _argv[SHELL_MAX_ARGS][SHELL_ARG_LEN];
        char _path[MAX_PATH];

        uint32_t _frameTimeMs = 16;
        bool open(const char* path);

};