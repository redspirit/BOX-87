#include "audio.h"
#include "palette.h"
#include <driver/i2s.h>


Audio::Audio(VGA& vga, const char* fullPath): 
    _vga(vga), _sd(), _tiles(), _kb() {

    strncpy(_path, fullPath, MAX_PATH);
    _path[MAX_PATH - 1] = 0;
}

Audio::~Audio() {
    _sd.close();
}

// Статическая функция-обертка, которую вызовет FreeRTOS
void Audio::audioTask(void* pvParameters) {
    Audio* instance = (Audio*)pvParameters;
    
    while (true) {
        // Если пауза — просто ждем 10мс и проверяем снова
        if (!instance->_isPlaying) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Если файл закончился — выходим или зацикливаем
        if (!instance->_sd.available()) {
            // instance->_sd.rewind(); // Раскомментируй для автоповтора
            break; 
        }

        size_t bytesRead = instance->_sd.read(instance->_pcmBuffer, BUFFER_SAMPLES * sizeof(int16_t));
        if (bytesRead > 0) {
            size_t written = 0;
            i2s_write(I2S_NUM_0, instance->_pcmBuffer, bytesRead, &written, portMAX_DELAY);
        }
    }

    instance->_isPlaying = false;
    instance->_playTaskHandle = nullptr;
    vTaskDelete(NULL);
}

bool Audio::init() {
    _vga.clear(0);
    paletteInit();
    _kb.init();
    _tiles.init(_vga, 8, 8);
    _tiles.setTransparent(false);

    if (!_sd.init()) {
        _tiles.print("Err _sd.init()", 1, 1, COLOR_RED);
        return false;
    }

    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 11025,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S, 
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 512,
        .use_apll = false,
        .tx_desc_auto_clear = true
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

    // указывает переданный в параметрах файл
    if (!play("/titans.pcm")) {
    // if (!play(_path)) {
        _tiles.print("Failed to open PCM file", 1, 1, COLOR_RED);
        _tiles.print(_path, 1, 2, COLOR_RED);
        return false;
    }

    _tiles.print("Play PCM file", 1, 1, COLOR_GREEN);

    _tiles.render();
    _vga.show();

    _vga.clear(0);
    _vga.show();

    return true;
}

void Audio::update(float dt) {
    if (_kb.isJustPressed(Keyboard::ESC)) requestExit();
    if (_kb.isJustPressed(Keyboard::SPACE)) {
        _isPlaying = !_isPlaying;
    }

    // --- ЗВУКОВАЯ СЕКЦИЯ ---
    if (_isPlaying && _sd.available()) {
        // Вычисляем, сколько байт нужно отправить для 22050 Гц при текущем dt
        // Обычно это константа для 60 FPS, но лучше считать через dt для надежности
        int samplesToPlay = (int)(11025 * dt); 
        size_t bytesToRead = samplesToPlay * sizeof(int16_t);

        // Ограничиваем размером нашего буфера
        if (bytesToRead > BUFFER_SAMPLES * sizeof(int16_t)) 
            bytesToRead = BUFFER_SAMPLES * sizeof(int16_t);

        size_t bytesRead = _sd.read(_pcmBuffer, bytesToRead);
        
        if (bytesRead > 0) {
            size_t written = 0;
            // ВАЖНО: таймаут должен быть 0 или очень маленький, 
            // чтобы I2S не блокировал отрисовку VGA
            i2s_write(I2S_NUM_0, _pcmBuffer, bytesRead, &written, 0);
        }
    }
    // -----------------------

    char fpsbuf[8];
    dtostrf(1 / dt, 0, 1, fpsbuf);
    _tiles.print(fpsbuf, 1, 3, COLOR_CYAN);

    _tiles.render();
    _vga.show();

    _kb.beginFrame();
}

void Audio::tick() {
    _kb.poll();
}

bool Audio::play(const char* path) {
    stop(); // Остановить текущий трек, если он играет

    if (!_sd.open(path)) {
        return false;
    }

    if (_sd.file() == nullptr)
        return false;

    _isPlaying = true;

    // Создаем задачу:
    // 1. Имя функции, 2. Имя для отладки, 3. Стек (4Кб хватит), 4. Параметр (this), 5. Приоритет, 6. Хендл
    //xTaskCreate(audioTask, "AudioTask", 4096, this, 5, &_playTaskHandle);

    return true;
}

void Audio::stop() {
    _isPlaying = false;
    // if (_playTaskHandle != nullptr) {
    //     // Даем немного времени задаче самой завершиться
    //     vTaskDelay(pdMS_TO_TICKS(50)); 
    // }
    _sd.close();
}