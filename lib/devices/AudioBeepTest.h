#pragma once

#include <stdint.h>

/*
    AudioBeepTest
    --------------
    Минимальный I2S beep-тест для проверки:
    - I2S линий
    - MAX9835A / MAX98357A
    - динамика и питания

    Формат:
    - Mono
    - PCM signed 16-bit
    - 22050 Hz
*/

namespace AudioBeepTest {

    // Инициализация I2S (вызывать один раз)
    void init();

    // Воспроизвести beep заданной длительности (в миллисекундах)
    void play(int durationMs);

    // Остановить вывод и очистить DMA
    void stop();

}
