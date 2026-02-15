#pragma once
#include <cstdint>

#define VGA_PIN_DATA_0  10
#define VGA_PIN_DATA_1  14

#define VGA_PIN_DATA_2  7
#define VGA_PIN_DATA_3  9
#define VGA_PIN_DATA_4  8

#define VGA_PIN_DATA_5  4
#define VGA_PIN_DATA_6  5
#define VGA_PIN_DATA_7  6

#define VGA_PIN_HSYNC   12
#define VGA_PIN_VSYNC   13

#define VGA_PIN_PCLK   -1
#define VGA_PIN_DE     -1

#define VGA_WIDTH      640
#define VGA_HEIGHT     480

namespace VGA {
    // Инициализация VGA, буферов и VSync
    void init();

    // Очистка текущего (заднего) буфера цветом
    void clear(uint8_t color = 0);
    void clearAll(uint8_t color = 0);

    // Рисование пикселя в задний буфер
    void dot(int x, int y, uint8_t color);

    // Рисование закрашенного прямоугольника
    void fillRect(int x, int y, int w, int h, uint8_t color);

    // Главная функция: ждет VSync и меняет буферы местами (Flip)
    void show();

    // Доступ к ширине/высоте
    inline int width() { return VGA_WIDTH; }
    inline int height() { return VGA_HEIGHT; }

    uint8_t* getBuffer();
}