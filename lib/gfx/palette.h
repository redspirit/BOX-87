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

void paletteInit();
uint8_t getColorByPalette(uint8_t index);
uint8_t getRgb322(uint8_t r, uint8_t g, uint8_t b);