#pragma once
#include <stdint.h>

#define PALETTE_SIZE 256

// диапазоны
#define PAL_TRANSPARENT   0
#define PAL_SYSTEM_START  1
#define PAL_SYSTEM_END    15
#define PAL_GRAY_START    16
#define PAL_GRAY_END      31
#define PAL_RGB_START     32
#define PAL_RGB_END       95

// индексы цветов
#define COLOR_BLACK       0
#define COLOR_WHITE       1
#define COLOR_RED         2
#define COLOR_GREEN       3
#define COLOR_BLUE        4
#define COLOR_YELLOW      5
#define COLOR_ORANGE      6
#define COLOR_PURPLE      7
#define COLOR_CYAN        8
#define COLOR_LIGHT_GRAY  9
#define COLOR_GRAY        10

void paletteInit();
uint8_t getColorByPalette(uint8_t index);
static inline uint8_t rgb332(uint8_t r, uint8_t g, uint8_t b) {
    return (r >> 5) | ((g >> 5) << 3) | (b & 0b11000000);
}
static inline uint16_t rgb333(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t r3 = r >> 5;
    uint16_t g3 = g >> 5;
    uint16_t b3 = b >> 5;
    return (r3) | (g3 << 3) | (b3 << 6);
}
