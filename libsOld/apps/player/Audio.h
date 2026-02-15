#pragma once

#include "SdReadBuffer.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2s.h>

#define PIN_DIN  35
#define PIN_RCLK 36
#define PIN_LRC  37

class Audio {
public:
    Audio();
    ~Audio();

    bool init(SdReadBuffer* rb,
              uint32_t audioOffset,
              uint32_t audioSamples,
              uint32_t sampleRate);

    void start();
    void stop();
    void pause(bool enable);

    void syncToFrame(uint32_t frame, uint32_t fps);

    bool isPlaying() const { return _playing; }

private:
    static void audioTaskEntry(void* arg);
    void audioTask();

private:
    SdReadBuffer* _rb;

    uint32_t _audioOffset;
    uint32_t _audioSamples;
    uint32_t _sampleRate;

    volatile bool _playing;
    volatile bool _paused;

    TaskHandle_t _task;

    uint32_t _samplesPlayed;

    // ring buffer
    static constexpr int PCM_BUF_SAMPLES = 2048;
    int16_t _pcmBuf[PCM_BUF_SAMPLES];
    int _pcmFill;
};
