#include "Audio.h"
#include <string.h>

Audio::Audio()
    : _rb(nullptr),
      _audioOffset(0),
      _audioSamples(0),
      _sampleRate(0),
      _playing(false),
      _paused(false),
      _task(nullptr),
      _samplesPlayed(0),
      _pcmFill(0) {
}

Audio::~Audio() {
    stop();
}

bool Audio::init(SdReadBuffer* rb,
                 uint32_t audioOffset,
                 uint32_t audioSamples,
                 uint32_t sampleRate) {

    _rb = rb;
    _audioOffset = audioOffset;
    _audioSamples = audioSamples;
    _sampleRate = sampleRate;

    // --- I2S init ---
    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate = sampleRate;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = 8;
    cfg.dma_buf_len = 512;
    cfg.use_apll = false;
    cfg.tx_desc_auto_clear = true;

    i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);

    i2s_pin_config_t pins = {
        .bck_io_num = PIN_RCLK,
        .ws_io_num = PIN_LRC,
        .data_out_num = PIN_DIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    i2s_set_pin(I2S_NUM_0, &pins);
    i2s_zero_dma_buffer(I2S_NUM_0);

    // позиционируемся на начало аудио
    _rb->seek(_audioOffset);

    // создаём RTOS-задачу
    xTaskCreatePinnedToCore(
        audioTaskEntry,
        "audio",
        4096,
        this,
        2,
        &_task,
        1
    );

    return true;
}

void Audio::start() {
    _playing = true;
    _paused = false;
}

void Audio::stop() {
    _playing = false;
    if (_task) {
        vTaskDelete(_task);
        _task = nullptr;
    }
    i2s_driver_uninstall(I2S_NUM_0);
}

void Audio::pause(bool enable) {
    _paused = enable;
}

void Audio::syncToFrame(uint32_t frame, uint32_t fps) {
    uint32_t expected =
        (uint64_t)frame * _sampleRate / fps;

    // если аудио убежало — притормозим
    if (_samplesPlayed > expected + (_sampleRate / 50)) {
        _paused = true;
    } else if (_paused && _samplesPlayed <= expected) {
        _paused = false;
    }
}

void Audio::audioTaskEntry(void* arg) {
    static_cast<Audio*>(arg)->audioTask();
}

void Audio::audioTask() {

    while (_playing) {

        if (_paused) {
            vTaskDelay(1);
            continue;
        }

        // дочитать PCM если надо
        if (_pcmFill < PCM_BUF_SAMPLES) {
            int need = PCM_BUF_SAMPLES - _pcmFill;
            int got = _rb->readBytes(
                &_pcmBuf[_pcmFill],
                need * sizeof(int16_t)
            ) / 2;

            _pcmFill += got;
        }

        if (_pcmFill == 0) {
            vTaskDelay(1);
            continue;
        }

        size_t written = 0;
        i2s_write(
            I2S_NUM_0,
            _pcmBuf,
            _pcmFill * sizeof(int16_t),
            &written,
            portMAX_DELAY
        );

        int played = written / 2;
        _samplesPlayed += played;

        // сдвигаем ring
        memmove(
            _pcmBuf,
            &_pcmBuf[played],
            (_pcmFill - played) * sizeof(int16_t)
        );
        _pcmFill -= played;
    }

    vTaskDelete(nullptr);
}
