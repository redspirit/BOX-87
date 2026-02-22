#include "BMPReader.h"
#include <esp_heap_caps.h>

BMPReader::BMPReader()
: lineBuffer_(nullptr),
  lineBufferSize_(0)
{
}

BMPReader::~BMPReader()
{
    if (lineBuffer_)
        heap_caps_free(lineBuffer_);
}


// ============================================================
// Чтение только заголовка
// ============================================================

bool BMPReader::readHeader(File& file, int& w, int& h)
{
    if (!file)
        return false;

    file.seek(0);

    uint8_t sig[2];
    if (file.read(sig, 2) != 2)
        return false;

    if (sig[0] != 'B' || sig[1] != 'M')
        return false;

    file.seek(14); // начало INFOHEADER

    uint32_t headerSize = read32(file);
    if (headerSize != 40)
        return false;

    int32_t width  = (int32_t)read32(file);
    int32_t height = (int32_t)read32(file);

    if (width <= 0 || height == 0)
        return false;

    if (height < 0)
        height = -height;

    w = width;
    h = height;

    file.seek(0); // возвращаем указатель

    return true;
}


// ============================================================
// Полное чтение
// ============================================================

bool BMPReader::read(File& file,
                     uint8_t* dst,
                     size_t dstSize)
{
    if (!file)
        return false;

    file.seek(0);

    // === FILE HEADER ===
    uint8_t sig[2];
    if (file.read(sig, 2) != 2)
        return false;

    if (sig[0] != 'B' || sig[1] != 'M')
        return false;

    file.seek(10);
    uint32_t dataOffset = read32(file);

    // === INFO HEADER ===
    uint32_t headerSize = read32(file);
    if (headerSize != 40)
        return false;

    int32_t width  = (int32_t)read32(file);
    int32_t height = (int32_t)read32(file);

    file.seek(2, SeekCur); // planes

    uint16_t bpp = read16(file);
    uint32_t compression = read32(file);

    if (compression != 0)
        return false;

    bool topDown = false;
    if (height < 0)
    {
        topDown = true;
        height = -height;
    }

    if (dstSize < (size_t)(width * height))
        return false;

    size_t rowSize = ((width * bpp + 31) / 32) * 4;

    // === Выделение lineBuffer в PSRAM ===
    if (rowSize > lineBufferSize_)
    {
        if (lineBuffer_)
            heap_caps_free(lineBuffer_);

        lineBuffer_ = (uint8_t*)heap_caps_malloc(
            rowSize,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        if (!lineBuffer_)
            return false;

        lineBufferSize_ = rowSize;
    }

    // === Палитра ===
    if (bpp == 8)
    {
        file.seek(14 + 40);

        for (int i = 0; i < 256; i++)
        {
            uint8_t B = file.read();
            uint8_t G = file.read();
            uint8_t R = file.read();
            file.read();

            palette_[i] = rgb888_to_b2g3r3(R, G, B);
        }
    }
    else if (bpp != 24)
    {
        return false;
    }

    file.seek(dataOffset);

    for (int y = 0; y < height; y++)
    {
        if (file.read(lineBuffer_, rowSize) != (int)rowSize)
            return false;

        int dstY = topDown ? y : (height - 1 - y);
        uint8_t* dstLine = dst + dstY * width;

        if (bpp == 8)
        {
            for (int x = 0; x < width; x++)
                dstLine[x] = palette_[lineBuffer_[x]];
        }
        else
        {
            for (int x = 0; x < width; x++)
            {
                uint8_t B = lineBuffer_[x * 3 + 0];
                uint8_t G = lineBuffer_[x * 3 + 1];
                uint8_t R = lineBuffer_[x * 3 + 2];

                dstLine[x] = rgb888_to_b2g3r3(R, G, B);
            }
        }
    }

    return true;
}


// ============================================================

uint16_t BMPReader::read16(File& f)
{
    uint16_t v;
    f.read((uint8_t*)&v, 2);
    return v;
}

uint32_t BMPReader::read32(File& f)
{
    uint32_t v;
    f.read((uint8_t*)&v, 4);
    return v;
}

uint8_t BMPReader::rgb888_to_b2g3r3(uint8_t R,
                                    uint8_t G,
                                    uint8_t B)
{
    uint8_t r = R >> 5;
    uint8_t g = G >> 5;
    uint8_t b = B >> 6;

    return (b << 6) | (g << 3) | r;
}