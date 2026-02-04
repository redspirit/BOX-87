#include <AudioBeepTest.h>
#include <driver/i2s.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr int SAMPLE_RATE = 22050;
static constexpr int TONE_FREQ   = 440;   // A4
static constexpr int AMPLITUDE   = 8000;  // не на максимум
static constexpr int BUFFER_SAMPLES = 512;

static int16_t pcmBuffer[BUFFER_SAMPLES];

void AudioBeepTest::init() {
    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        // Исправлено: используем актуальную константу
        .communication_format = I2S_COMM_FORMAT_STAND_I2S, 
        // Исправлено: перенесено выше для соблюдения порядка в i2s_config_t
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 512,
        .use_apll = false,
        .tx_desc_auto_clear = true // Полезно: автоматически очищает буфер, если данных нет
    };

    i2s_pin_config_t pins = {
        .bck_io_num = 14,
        .ws_io_num  = 3,
        .data_out_num = 21,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
    i2s_set_pin(I2S_NUM_0, &pins);
    i2s_zero_dma_buffer(I2S_NUM_0);
}

void AudioBeepTest::play(int durationMs) {
    float phase = 0.0f;
    float phaseStep = 2.0f * M_PI * TONE_FREQ / SAMPLE_RATE;

    int totalSamples = (SAMPLE_RATE * durationMs) / 1000;

    while (totalSamples > 0) {
        int samplesNow = (totalSamples > BUFFER_SAMPLES)
            ? BUFFER_SAMPLES
            : totalSamples;

        for (int i = 0; i < samplesNow; ++i) {
            pcmBuffer[i] = (int16_t)(sinf(phase) * AMPLITUDE);
            phase += phaseStep;
            if (phase >= 2.0f * M_PI)
                phase -= 2.0f * M_PI;
        }

        size_t written;
        i2s_write(
            I2S_NUM_0,
            pcmBuffer,
            samplesNow * sizeof(int16_t),
            &written,
            portMAX_DELAY
        );

        totalSamples -= samplesNow;
    }
}
