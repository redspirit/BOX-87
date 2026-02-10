#include "player.h"
#include <esp32-hal-psram.h>
#include <palette.h>
#include <string.h>
#include <LOG.h>

// -----------------------------------------------------------------------------
// Utils
// -----------------------------------------------------------------------------

static uint16_t readU16(SdReadBuffer* rb) {
    uint8_t lo = rb->readU8();
    uint8_t hi = rb->readU8();
    return lo | (hi << 8);
}

static uint32_t readU32(SdReadBuffer* rb) {
    uint32_t v = 0;
    v |= rb->readU8();
    v |= rb->readU8() << 8;
    v |= rb->readU8() << 16;
    v |= rb->readU8() << 24;
    return v;
}

// -----------------------------------------------------------------------------
// Ctor / Dtor
// -----------------------------------------------------------------------------

Player::Player(VGA& vga, const ShellParser& args, const char* fullPath)
    : _vga(vga),
      _sd(),
      _tiles(),
      _kb(),
      _rb(nullptr),
      _argc(args.argc),
      currentFrame(0),
      _isPause(false),
      stateFB(nullptr) {

    for (int i = 0; i < _argc; ++i) {
        strncpy(_argv[i], args.argv[i], SHELL_ARG_LEN);
        _argv[i][SHELL_ARG_LEN - 1] = 0;
    }

    strncpy(_path, fullPath, MAX_PATH);
    _path[MAX_PATH - 1] = 0;
}

Player::~Player() {

    if (stateFB) {
        heap_caps_free(stateFB);
        stateFB = nullptr;
    }

    if (_rb) {
        delete _rb;
        _rb = nullptr;
    }

    _sd.close();
}

// -----------------------------------------------------------------------------
// Init
// -----------------------------------------------------------------------------

bool Player::init() {
    _vga.clear(0);
    _kb.init();

    _tiles.init(_vga, 8, 8);
    _tiles.setTransparent(false);

    if (!_sd.init()) {
        _tiles.print("SD init failed", 1, 1, COLOR_RED);
        return false;
    }

    if (!open(_path)) {
        _tiles.print("Failed to open RV file", 1, 1, COLOR_RED);
        return false;
    }

    _vga.clear(0);
    _vga.show();
    _vga.clear(0);
    _vga.show();
    return true;
}

// -----------------------------------------------------------------------------
// Update / Tick
// -----------------------------------------------------------------------------

void Player::update(float dt) {

    if (_kb.isJustPressed(Keyboard::ESC)) {
        requestExit();
    }

    if (_kb.isJustPressed(Keyboard::SPACE)) {
        doPause();
    }

    if (!_isPause && !isFinished()) {
        playFrame();

        char buf[16];
        itoa(currentFrame, buf, 10);
        _tiles.print(buf, 0, 29, COLOR_GREEN);

        char fpsbuf[8];
        dtostrf(1 / dt, 0, 1, fpsbuf);
        _tiles.print(fpsbuf, 11, 29, COLOR_WHITE);

        _tiles.render();
        _vga.show();
    }

    _kb.beginFrame();
}

void Player::tick() {
    _kb.poll();
}

void Player::doPause() {
    _isPause = !_isPause;
}

// -----------------------------------------------------------------------------
// File open
// -----------------------------------------------------------------------------

bool Player::open(const char* path) {

    LOG.println("Do open");

    if (!_sd.open(path)) {
        return false;
    }

    if (_sd.file() == nullptr) {
        return false;
    }

    _rb = new SdReadBuffer(_sd.file());

    LOG.println("before read header");

    if (!readHeader()) {
        return false;
    }

    LOG.println("after read header");

    tilesX = width / tileW;
    tilesY = height / tileH;

    currentFrame = 0;
    _isPause = false;

    // jump to video stream
    _rb->seek(videoOffset);

    return true;
}

// -----------------------------------------------------------------------------
// Header
// -----------------------------------------------------------------------------

bool Player::readHeader() {

    char magic[2];
    _rb->readBytes(magic, 2);

    if (memcmp(magic, "RV", 2) != 0) {
        _tiles.print("Not RV file", 1, 1, COLOR_RED);
        return false;
    }

    uint8_t version = _rb->readU8();
    if (version != 4) {
        _tiles.print("Need RV v4", 1, 1, COLOR_RED);
        return false;
    }

    width       = readU16(_rb);
    height      = readU16(_rb);
    fps         = readU16(_rb);
    frameCount  = readU32(_rb);
    bpp         = _rb->readU8();
    tileW       = _rb->readU8();
    tileH       = _rb->readU8();
    videoOffset = readU32(_rb);

    // skip audio info
    readU32(_rb); // audioOffset
    readU32(_rb); // audioRate
    readU32(_rb); // audioSamples

    if (bpp != 8) {
        _tiles.print("Only 8bpp supported", 1, 1, COLOR_RED);
        return false;
    }

    stateFB = (uint8_t*)ps_malloc(width * height);
    if (!stateFB) {
        _tiles.print("No PSRAM", 1, 1, COLOR_RED);
        return false;
    }

    memset(stateFB, 0, width * height);

    _frameTimeMs = 1000 / fps;

    if (height < _vga.height()) {
        _vgaYOffset = (_vga.height() - height) / 2;
    } else {
        _vgaYOffset = 0;
    }

    return true;
}

// -----------------------------------------------------------------------------
// Frame decode
// -----------------------------------------------------------------------------

void Player::unpackTile(uint16_t tileIndex, const uint8_t* data) {

    uint16_t tx = tileIndex % tilesX;
    uint16_t ty = tileIndex / tilesX;

    int px = tx * tileW;
    int py = ty * tileH;

    const uint8_t* src = data;

    for (int y = 0; y < tileH; y++) {
        memcpy(
            &stateFB[(py + y) * width + px],
            src,
            tileW
        );
        src += tileW;
    }
}

void Player::playFrame() {

    if (!_rb->available()) {
        return;
    }

    uint8_t flags = _rb->readU8();
    (void)flags;

    uint16_t updateCount = readU16(_rb);

    uint16_t tileIndex = 0;
    static uint8_t tileBuf[64];

    for (uint16_t i = 0; i < updateCount; i++) {

        uint8_t skip;
        do {
            skip = _rb->readU8();
            tileIndex += skip;
        } while (skip == 255);

        _rb->readBytes(tileBuf, tileW * tileH);
        unpackTile(tileIndex, tileBuf);
        tileIndex++;
    }

    currentFrame++;

    // blit framebuffer to VGA
    for (int y = 0; y < height; y++) {
        memcpy(
            _vga.getLinePtr8(y + _vgaYOffset),
            &stateFB[y * width],
            width
        );
    }
}

bool Player::isFinished() const {
    return currentFrame >= frameCount;
}
