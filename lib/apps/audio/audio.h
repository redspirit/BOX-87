#pragma once
#include "ISubsystem.h"
#include "SDCard.h"
#include "VGA.h"
#include "TextTiles.h"
#include "keyboard.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define MAX_PATH 128
#define BUFFER_SAMPLES 512

class Audio : public ISubsystem {
    public:
        Audio(VGA& _vga, const char* fullPath);
        ~Audio();

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
        Keyboard _kb;

        char _path[MAX_PATH];

        bool play(const char* path);
        void stop();

        static void audioTask(void* pvParameters); // Обертка для FreeRTOS
        TaskHandle_t _playTaskHandle = nullptr;
        bool _isPlaying = false;

        int16_t _pcmBuffer[BUFFER_SAMPLES];

        // playback
        uint32_t _frameTimeMs = 16;

};