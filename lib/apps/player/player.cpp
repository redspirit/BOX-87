#include "player.h"
#include "palette.h"
#include <esp32-hal-psram.h>

// ------------------------------------------------------------
// helpers
// ------------------------------------------------------------

static uint16_t readU16(SDCard& sd) {
    uint8_t lo, hi;
    sd.read(&lo, 1);
    sd.read(&hi, 1);
    return lo | (hi << 8);
}

Player::Player(VGA& vga, const ShellParser& args, const char* fullPath): _vga(vga), _sd(), _tiles(), _argc(args.argc) {
    for (int i = 0; i < _argc; ++i) {
        strncpy(_argv[i], args.argv[i], SHELL_ARG_LEN);
        _argv[i][SHELL_ARG_LEN - 1] = 0;
    }
    
    strncpy(_path, fullPath, MAX_PATH);
    _path[MAX_PATH - 1] = 0;
}

Player::~Player() {
    if (_rb) {
        delete _rb;
        _rb = nullptr;
    }
    _sd.close();
}

bool Player::init() {
    _vga.clear(0);
    paletteInit();
    _tiles.init(_vga, 8, 8);
    _tiles.setTransparent(false);

    if (!_sd.init()) {
        _tiles.print("Err _sd.init()", 1, 1, COLOR_RED);
        return false;
    }

    // указывает переданный в параметрах файл
    if (!open(_path)) {
        _tiles.print("Failed to open RVV file", 1, 1, COLOR_RED);
        _tiles.print(_argv[1], 1, 2, COLOR_RED);
        return false;
    }

    _tiles.render();
    _vga.show();

    _vga.clear(0);
    _vga.show();

    return true;
}

void Player::update(float dt) {
    if (!isFinished()) {
        playFrame();

        char framebuf[8];
        itoa(currentFrame, framebuf, 10);
        _tiles.print(framebuf, 0, 29, COLOR_GREEN);

        char dtbuf[8];
        dtostrf(dt, 0, 3, dtbuf);
        _tiles.print(dtbuf, 5, 29, COLOR_BLUE);

        char fpsbuf[8];
        dtostrf(1 / dt, 0, 1, fpsbuf);
        _tiles.print(fpsbuf, 11, 29, COLOR_CYAN);

        _tiles.render();
        _vga.show();
    }
}

void Player::tick() {

}

bool Player::open(const char* path) {
    if (!_sd.open(path)) {
        return false;
    }

    if (_sd.file() == nullptr)
        return false;

    _rb = new SdReadBuffer(_sd.file());

    if (!readHeader()) {
        return false;
    }

    readPalette();

    tilesX = width / tileW;
    tilesY = height / tileH;

    currentFrame = 0;

    return true;
}

bool Player::readHeader() {
    char magic[3];
    _rb->readBytes(magic, 3);

    if (memcmp(magic, "RVV", 3) != 0) {
        _tiles.print("Not an RVV file", 1, 1, COLOR_RED);
        return false;
    }

    uint8_t version = _rb->readU8();
    if (version < 5) {
        _tiles.print("Unsupported RVV version", 1, 1, COLOR_RED);
        return false;
    }

    width  = _rb->readU16();
    height = _rb->readU16();

    tileW = _rb->readU8();
    tileH = _rb->readU8();
    bpp   = _rb->readU8();
    fps   = _rb->readU8();

    frameCount       = _rb->readU16();
    paletteSize      = _rb->readU16();
    keyframeInterval = _rb->readU16();

    stateFB = (uint8_t*)ps_malloc(width * height);
    memset(stateFB, 0, width * height);

    directColor = (bpp == 8);

    _frameTimeMs = 1000 / fps;

    return true;
}

void Player::readPalette() {
    for (int i = 0; i < paletteSize; i++) {
        uint8_t r = _rb->readU8();
        uint8_t g = _rb->readU8();
        uint8_t b = _rb->readU8();
        palette[i] = (r >> 5) | ((g >> 5) << 3) | (b & 0b11000000);
    }
}

void Player::unpackTile(uint16_t tileIndex, const uint8_t* data) {
    uint16_t tx = tileIndex % tilesX;
    uint16_t ty = tileIndex / tilesX;

    int px = tx * tileW;
    int py = ty * tileH;

    // ------------------------------------------------------------
    // 8 BPP — DIRECT COLOR (RGB332)
    // ------------------------------------------------------------
    if (directColor) {
        const uint8_t* src = data;

        for (int y = 0; y < tileH; y++) {
            memcpy(
                &stateFB[(py + y) * width + px],
                src,
                tileW
            );
            src += tileW;
        }
        return;
    }

    // ------------------------------------------------------------
    // Indexed modes (1/2/4 bpp)
    // ------------------------------------------------------------
    int bit = 0;
    int byte = 0;

    for (int y = 0; y < tileH; y++) {
        uint8_t* line = &stateFB[(py + y) * width];

        for (int x = 0; x < tileW; x++) {
            uint8_t v = 0;

            for (int b = 0; b < bpp; b++) {
                v = (v << 1) | ((data[byte] >> (7 - bit)) & 1);
                bit++;
                if (bit == 8) {
                    bit = 0;
                    byte++;
                }
            }

            line[px + x] = palette[v];
        }
    }
}


void Player::playFrame() {
    if (!_rb->available())
        return;

    uint8_t flags = _rb->readU8();
    uint16_t updateCount = _rb->readU16();

    uint16_t tileIndex = 0;

    static uint8_t tileBuf[64];
    const int bytesPerTile = directColor ? (tileW * tileH) : ((tileW * tileH * bpp) >> 3);

    for (uint16_t i = 0; i < updateCount; i++) {
        uint8_t skip;
        do {
            skip = _rb->readU8();
            tileIndex += skip;
        } while (skip == 255);

        _rb->readBytes(tileBuf, bytesPerTile);
        unpackTile(tileIndex, tileBuf);
        tileIndex++;
    }

    currentFrame++;

    for (int y = 0; y < height; y++) {
        memcpy(
            _vga.getLinePtr8(y),
            &stateFB[y * width],
            width
        );
    }

}


bool Player::isFinished() const {
    return currentFrame >= frameCount;
}