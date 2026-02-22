#include "PNGReader.h"
#include "lodepng.h"
#include <esp_heap_caps.h>

PNGReader::PNGReader()
: fileBuffer_(nullptr),
  fileSize_(0),
  decodeBuffer_(nullptr),
  decodeSize_(0)
{
}

PNGReader::~PNGReader()
{
    freeBuffers();
}

void PNGReader::freeBuffers()
{
    if (fileBuffer_)
        heap_caps_free(fileBuffer_);
    if (decodeBuffer_)
        heap_caps_free(decodeBuffer_);

    fileBuffer_ = nullptr;
    decodeBuffer_ = nullptr;
}

bool PNGReader::loadFileToPSRAM(File& file)
{
    fileSize_ = file.size();

    fileBuffer_ = (uint8_t*)heap_caps_malloc(
        fileSize_,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!fileBuffer_)
        return false;

    file.read(fileBuffer_, fileSize_);
    return true;
}


// ============================================================
// HEADER
// ============================================================

bool PNGReader::readHeader(File& file, int& w, int& h)
{
    if (!file)
        return false;

    file.seek(0);

    if (!loadFileToPSRAM(file))
        return false;

    unsigned width = 0;
    unsigned height = 0;

    LodePNGState state;
    lodepng_state_init(&state);

    unsigned error = lodepng_inspect(
        &width,
        &height,
        &state,
        fileBuffer_,
        fileSize_);

    lodepng_state_cleanup(&state);

    heap_caps_free(fileBuffer_);
    fileBuffer_ = nullptr;

    if (error)
        return false;

    w = width;
    h = height;

    return true;
}


// ============================================================
// FULL READ
// ============================================================

bool PNGReader::read(File& file,
                     uint8_t* dst,
                     size_t dstSize)
{
    if (!file)
        return false;

    file.seek(0);

    if (!loadFileToPSRAM(file))
        return false;

    unsigned width, height;
    unsigned char* image = nullptr;

    unsigned error = lodepng_decode32(
        &image,
        &width,
        &height,
        fileBuffer_,
        fileSize_);

    heap_caps_free(fileBuffer_);
    fileBuffer_ = nullptr;

    if (error)
        return false;

    size_t required = width * height;
    if (dstSize < required)
    {
        free(image);
        return false;
    }

    // Конвертация RGBA → B2G3R3
    for (unsigned i = 0; i < width * height; i++)
    {
        uint8_t R = image[i * 4 + 0];
        uint8_t G = image[i * 4 + 1];
        uint8_t B = image[i * 4 + 2];

        dst[i] = rgb888_to_b2g3r3(R, G, B);
    }

    free(image);

    return true;
}


// ============================================================

uint8_t PNGReader::rgb888_to_b2g3r3(uint8_t R,
                                    uint8_t G,
                                    uint8_t B)
{
    uint8_t r = R >> 5;
    uint8_t g = G >> 5;
    uint8_t b = B >> 6;

    return (b << 6) | (g << 3) | r;
}