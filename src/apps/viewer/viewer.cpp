#include "viewer.h"
#include <esp32-hal-psram.h>
#include <palette.h>
#include <string.h>
#include <LOG.h>
#include "sdcard.h"
#include "VGA/VGA.h"
#include "keyboard.h"
#include <BMPReader.h>
#include <PNGReader.h>
#include <JPGReader.h>

Viewer::Viewer(const char* fullPath)
    : _tiles() {

    strncpy(_path, fullPath, MAX_PATH);
    _path[MAX_PATH - 1] = 0;
}

Viewer::~Viewer() {
    SDCARD::close();
}

// -----------------------------------------------------------------------------
// Init
// -----------------------------------------------------------------------------

bool Viewer::init() {
    _tiles.init();
    _tiles.setTransparent(false);

    VGA::clearAll(0);

    if (!open(_path)) {
        _tiles.print("Failed to open image file", 1, 1, COLOR_RED);
        return false;
    }

    return true;
}

void Viewer::update(float dt) {
    if (KEYBOARD::isJustPressed(KEYBOARD::ESC)) {
        requestExit();
    }

    KEYBOARD::beginFrame();
}

void Viewer::tick() {

}

bool Viewer::hasExtension(const char* path, const char* ext) {
    const char* dot = strrchr(path, '.');
    if (!dot)
        return false;

    dot++; // пропустить точку

    while (*dot && *ext)
    {
        if (tolower((unsigned char)*dot) !=
            tolower((unsigned char)*ext))
            return false;

        dot++;
        ext++;
    }

    return (*dot == '\0' && *ext == '\0');
}

bool Viewer::open(const char* path) {
    int screenW = VGA::width();
    int screenH = VGA::height();

    if (!SDCARD::open(path, "r")) {
        return false;
    }

    if (SDCARD::getFile() == nullptr) {
        return false;
    }

    File* f = SDCARD::getFile();
    int w, h;
    uint8_t* img;

    if (hasExtension(path, "bmp")) {

        BMPReader reader;

        if (!reader.readHeader(*f, w, h)) {
            LOG.println("bmp read header failed");
            SDCARD::close();
            return false;
        }

        if (w > screenW || h > screenH) {
            _tiles.print("Image too big", 1, 1, COLOR_RED);
            SDCARD::close();
            return false;
        }

        size_t bufferSize = w * h;
        img = (uint8_t*)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        if (!img) {
            LOG.println("bmp buffer not created");
            SDCARD::close();
            return false;
        }

        if (!reader.read(*f, img, bufferSize)) {
            LOG.println("bmp read failed");
            heap_caps_free(img);
            img = nullptr;
            SDCARD::close();
        }

    } else if (hasExtension(path, "png")) {

        PNGReader reader;

        if (!reader.readHeader(*f, w, h)) {
            LOG.println("png read header failed");
            SDCARD::close();
            return false;
        }

        if (w > screenW || h > screenH) {
            _tiles.print("Image too big", 1, 1, COLOR_RED);
            SDCARD::close();
            return false;
        }

        size_t bufferSize = w * h;
        img = (uint8_t*)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        if (!img) {
            LOG.println("img buffer not created");
            SDCARD::close();
            return false;
        }

        if (!reader.read(*f, img, bufferSize)) {
            LOG.println("png read failed");
            heap_caps_free(img);
            img = nullptr;
            SDCARD::close();
        }

    } else if (hasExtension(path, "jpg") || hasExtension(path, "jpeg")) {

        JPGReader reader;

        if (!reader.readHeader(*f, w, h)) {
            LOG.println("jpg read header failed");
            SDCARD::close();
            return false;
        }

        if (w > screenW || h > screenH) {
            _tiles.print("Image too big", 1, 1, COLOR_RED);
            SDCARD::close();
            return false;
        }

        size_t bufferSize = w * h;
        img = (uint8_t*)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        if (!img) {
            LOG.println("img buffer not created");
            SDCARD::close();
            return false;
        }

        if (!reader.read(*f, img, bufferSize)) {
            LOG.println("jpg read failed");
            heap_caps_free(img);
            img = nullptr;
            SDCARD::close();
        }

    } else {
        _tiles.print("Unsupported format", 1, 1, COLOR_RED);
        return false;
    }

    int offsetX = (screenW - w) / 2;
    int offsetY = (screenH - h) / 2;

    // выравнимание по центру экрана
    for (int y = 0; y < h; y++) {
        uint8_t* dstLine = VGA::getLinePtr8(y + offsetY) + offsetX;
        uint8_t* srcLine = img + y * w;
        memcpy(dstLine, srcLine, w);
    }

    VGA::show();
    SDCARD::close();

    heap_caps_free(img);
    img = nullptr;

    return true;
}

