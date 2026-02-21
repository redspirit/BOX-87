#pragma once
#include <stdint.h>
#include <VGA/VGA.h>

namespace BMPScreen
{
    // Внутренний helper — запись little endian
    static inline void write16(File& f, uint16_t v)
    {
        f.write((uint8_t)(v & 0xFF));
        f.write((uint8_t)((v >> 8) & 0xFF));
    }

    static inline void write32(File& f, uint32_t v)
    {
        f.write((uint8_t)(v & 0xFF));
        f.write((uint8_t)((v >> 8) & 0xFF));
        f.write((uint8_t)((v >> 16) & 0xFF));
        f.write((uint8_t)((v >> 24) & 0xFF));
    }

    // Генерация RGB332 палитры
    static void writeRGB332Palette(File& f)
    {
        for (int i = 0; i < 256; i++)
        {
            // BBGGGRRR
            uint8_t b = (i >> 6) & 0x03;  // 2 бита
            uint8_t g = (i >> 3) & 0x07;  // 3 бита
            uint8_t r = (i >> 0) & 0x07;  // 3 бита

            // Расширение до 8 бит
            uint8_t R = (r * 255) / 7;
            uint8_t G = (g * 255) / 7;
            uint8_t B = (b * 255) / 3;

            // BMP хранит BGR0
            f.write(B);
            f.write(G);
            f.write(R);
            f.write((uint8_t)0);
        }
    }

    // === Основная функция ===
    static void makeScreenShot(File& file)
    {
        const int width  = VGA::width();
        const int height = VGA::height();

        const int rowSize = (width + 3) & ~3;   // выравнивание до 4 байт
        const int pixelDataSize = rowSize * height;
        const int headerSize = 14 + 40 + 1024;
        const int fileSize = headerSize + pixelDataSize;

        // =========================
        // BITMAPFILEHEADER (14)
        // =========================
        file.write('B');
        file.write('M');
        write32(file, fileSize);
        write32(file, 0);
        write32(file, headerSize);

        // =========================
        // BITMAPINFOHEADER (40)
        // =========================
        write32(file, 40);            // size
        write32(file, width);
        write32(file, height);
        write16(file, 1);             // planes
        write16(file, 8);             // bits per pixel
        write32(file, 0);             // compression (BI_RGB)
        write32(file, pixelDataSize);
        write32(file, 0);             // x ppm
        write32(file, 0);             // y ppm
        write32(file, 256);           // colors used
        write32(file, 0);             // important colors

        // =========================
        // Палитра RGB332
        // =========================
        writeRGB332Palette(file);

        // =========================
        // Пиксели (снизу вверх)
        // =========================
        uint8_t padding[3] = {0, 0, 0};
        const int pad = rowSize - width;

        for (int y = height - 1; y >= 0; y--)
        {
            const uint8_t* row = VGA::getLinePtr8(y);
            file.write(row, width);

            if (pad > 0)
                file.write(padding, pad);
        }
    }
}