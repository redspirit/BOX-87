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

Player::Player(VGA& vga): _vga(vga), _sd(), _console() {
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
    _console.init(_vga, 8, 8, COLOR_GREEN);
    _console.setCursorVisible(false);

    if (!_sd.init()) {
        _console.printLn("SD card not initialized");
        return false;
    }

    _console.printLn("Opening video...");

    if (!open("/badapple.rvv")) {
    // if (!open("/tetoris.rvv")) {
        _console.printLn("Failed to open RVV");
        return false;
    }

    _console.printLn("RVV player ready");


    _console.show();
    _vga.show();

    _vga.clear(0);
    _vga.show();

    return true;
}

void Player::update(float dt) {
    if (!isFinished()) {
        // _vga.clear(0);
        playFrame();
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
        _console.printLn("Not an RVV file");
        return false;
    }

    uint8_t version = _rb->readU8();
    if (version < 5) {
        _console.printLn("Unsupported RVV version");
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
    const int bytesPerTile = (tileW * tileH * bpp) >> 3;

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

    _vga.show();
}

bool Player::isFinished() const {
    return currentFrame >= frameCount;
}