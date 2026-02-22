#include "JPGReader.h"

JPGReader::JPGReader()
: file_(nullptr),
  dst_(nullptr),
  width_(0),
  height_(0)
{
}

JPGReader::~JPGReader()
{
}


// ============================================================
// READ HEADER
// ============================================================

bool JPGReader::readHeader(File& file, int& w, int& h)
{
    file.seek(0);
    file_ = &file;

    JDEC jd;

    JRESULT res = jd_prepare(
        &jd,
        inputFunc,
        workspace_,
        sizeof(workspace_),
        this);

    if (res != JDR_OK)
        return false;

    w = jd.width;
    h = jd.height;

    file.seek(0);
    return true;
}


// ============================================================
// FULL READ
// ============================================================

bool JPGReader::read(File& file,
                     uint8_t* dst,
                     size_t dstSize)
{
    file.seek(0);

    file_ = &file;
    dst_  = dst;

    JDEC jd;

    JRESULT res = jd_prepare(
        &jd,
        inputFunc,
        workspace_,
        sizeof(workspace_),
        this);

    if (res != JDR_OK)
        return false;

    width_  = jd.width;
    height_ = jd.height;

    if (dstSize < (size_t)width_ * height_)
        return false;

    res = jd_decomp(&jd, outputFunc, 0);

    file.seek(0);

    return (res == JDR_OK);
}


// ============================================================
// INPUT CALLBACK
// ============================================================

size_t JPGReader::inputFunc(JDEC* jd,
                            uint8_t* buff,
                            size_t nbyte)
{
    JPGReader* self = (JPGReader*)jd->device;

    if (!buff)
    {
        self->file_->seek(nbyte, SeekCur);
        return nbyte;
    }

    return self->file_->read(buff, nbyte);
}


// ============================================================
// OUTPUT CALLBACK
// ============================================================

int JPGReader::outputFunc(JDEC* jd,
                          void* bitmap,
                          JRECT* rect)
{
    JPGReader* self = (JPGReader*)jd->device;

    uint8_t* src = (uint8_t*)bitmap;

    for (int y = rect->top; y <= rect->bottom; y++)
    {
        for (int x = rect->left; x <= rect->right; x++)
        {
            int localX = x - rect->left;
            int localY = y - rect->top;

            int blockWidth =
                rect->right - rect->left + 1;

            int srcIndex =
                (localY * blockWidth + localX) * 3;

            uint8_t R = src[srcIndex + 0];
            uint8_t G = src[srcIndex + 1];
            uint8_t B = src[srcIndex + 2];

            self->dst_[y * self->width_ + x] =
                rgb888_to_b2g3r3(R, G, B);
        }
    }

    return 1; // продолжать декомпрессию
}


// ============================================================

uint8_t JPGReader::rgb888_to_b2g3r3(uint8_t R,
                                    uint8_t G,
                                    uint8_t B)
{
    uint8_t r = R >> 5;
    uint8_t g = G >> 5;
    uint8_t b = B >> 6;

    return (b << 6) | (g << 3) | r;
}