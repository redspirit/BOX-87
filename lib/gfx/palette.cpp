#include "palette.h"
#include "VGA.h"

uint8_t palette[PALETTE_SIZE];

static inline uint8_t rgb332(uint8_t r, uint8_t g, uint8_t b) {
	return (r >> 5) | ((g >> 5) << 3) | (b & 0b11000000);
}

// ------------------------------------------------------------
// ИНИЦИАЛИЗАЦИЯ ПАЛИТРЫ
// ------------------------------------------------------------
void paletteInit() {
    // 0 — прозрачный / чёрный
    palette[0] = rgb332(0, 0, 0);

    // --------------------------------------------------------
    // 1–15 : системные цвета
    // --------------------------------------------------------
    palette[1]  = rgb332(255, 255, 255); // white
    palette[2]  = rgb332(255, 0, 0);     // red
    palette[3]  = rgb332(0, 255, 0);     // green
    palette[4]  = rgb332(0, 0, 255);     // blue
    palette[5]  = rgb332(255, 255, 0);   // yellow
    palette[6]  = rgb332(255, 128, 0);   // orange
    palette[7]  = rgb332(128, 0, 255);   // purple
    palette[8]  = rgb332(0, 255, 255);   // cyan
    palette[9]  = rgb332(192, 192, 192); // light gray
    palette[10] = rgb332(128, 128, 128); // gray
    palette[11] = rgb332(64, 64, 64);    // dark gray
    palette[12] = rgb332(255, 128, 128); // light red
    palette[13] = rgb332(128, 255, 128); // light green
    palette[14] = rgb332(128, 128, 255); // light blue
    palette[15] = rgb332(0, 0, 0);       // black

    // --------------------------------------------------------
    // 16–31 : grayscale (16 уровней)
    // --------------------------------------------------------
    for (int i = 0; i < 16; i++) {
        uint8_t v = i * 17; // 0..255
        palette[PAL_GRAY_START + i] = rgb332(v, v, v);
    }

    // --------------------------------------------------------
    // 32–95 : RGB 4×4×4 (64 цвета)
    // --------------------------------------------------------
    int idx = PAL_RGB_START;
    for (int r = 0; r < 4; r++) {
        for (int g = 0; g < 4; g++) {
            for (int b = 0; b < 4; b++) {
                palette[idx++] = rgb332(
                    r * 85,
                    g * 85,
                    b * 85
                );
            }
        }
    }

    // --------------------------------------------------------
    // 96–255 : кастом (пока чёрный)
    // --------------------------------------------------------
    for (int i = 96; i < 256; i++) {
        palette[i] = rgb332(0, 0, 0);
    }

    // цвета логотипа
    palette[96] = rgb332(255, 255, 255);
    palette[97] = rgb332(0, 0, 0);
    palette[98] = rgb332(255, 0, 113);
    palette[99] = rgb332(0, 49, 255);
}

uint8_t getColorByPalette(uint8_t index) {
    return palette[index];
}