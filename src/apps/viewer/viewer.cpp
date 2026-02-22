#include "viewer.h"
#include <esp32-hal-psram.h>
#include <palette.h>
#include <string.h>
#include <LOG.h>
#include "sdcard.h"
#include "VGA/VGA.h"
#include "keyboard.h"
#include <BMPReader.h>

Viewer::Viewer(const ShellParser& args, const char* fullPath)
    : _tiles(),
      _argc(args.argc){

    for (int i = 0; i < _argc; ++i) {
        strncpy(_argv[i], args.argv[i], SHELL_ARG_LEN);
        _argv[i][SHELL_ARG_LEN - 1] = 0;
    }

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

bool Viewer::open(const char* path) {

    LOG.println("Do open");

    if (!SDCARD::open("/images/test_large.bmp", "r")) {
        return false;
    }

    if (SDCARD::getFile() == nullptr) {
        return false;
    }

    File* f = SDCARD::getFile();

    int w, h;

    BMPReader reader;

    if (!reader.readHeader(*f, w, h)) {
        LOG.println("BMP read header failed");
        return false;
    }

    size_t bufferSize = w * h;
    uint8_t* img = (uint8_t*)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!img) {
        LOG.println("img buffer not created");
        return false;
    }
    
    LOG.println("Read begin");
    if (!reader.read(*f, img, bufferSize)) {
        LOG.println("BMP read failed");
        heap_caps_free(img);
        img = nullptr;
    }


    int screenW = VGA::width();
    int screenH = VGA::height();
    int offsetX = (screenW - w) / 2;
    int offsetY = (screenH - h) / 2;

    for (int y = 0; y < h; y++) {
        uint8_t* dstLine = VGA::getLinePtr8(y + offsetY) + offsetX;
        uint8_t* srcLine = img + y * w;
        memcpy(dstLine, srcLine, w);
    }

    LOG.println("Do show");
    VGA::show();
    SDCARD::close();

    // тут тоже буффер img надо почистить

    return true;
}

