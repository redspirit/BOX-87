#pragma once

#include <Arduino.h>
#include "PinConfig.h"
#include "Mode.h"
#include "DMAVideoBuffer.h"

namespace VGA {
    // Инициализация и управление
    bool init(const Mode mode, int bits);
    bool start();
    bool show();
	void stop(); // Остановка железа
    bool changeMode(const Mode newMode, int newBits); // Смена режима

    // Свойства
    int width();
    int height();

    // Рисование (автоматически выбирает активный буфер)
    void clear(uint8_t color);
	void clearAll(uint8_t color); // Очистка всех буферов
    void fillRect8(int x, int y, int w, int h, int rgb);
    void fillRect16(int x, int y, int w, int h, int rgb);
    void dot8(int x, int y, int rgb);
    void dot16(int x, int y, int rgb);

    // Доступ к сырым данным
    uint8_t* getLinePtr8(int y);
    uint16_t* getLinePtr16(int y);
    uint8_t* getLinePtr8Safe(int y);

    // Блокировка для многозадачности (чтобы команды не рисовали одновременно)
    void lock();
    void unlock();
}