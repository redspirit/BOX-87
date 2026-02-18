#include "player.h"
#include <esp32-hal-psram.h>
#include <palette.h>
#include <string.h>
#include <LOG.h>
#include "sdcard.h"
#include "VGA/VGA.h"
#include "keyboard.h"

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

Player::Player(const ShellParser& args, const char* fullPath)
    : _tiles(),
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
    _audio.stop();

    if (stateFB) {
        heap_caps_free(stateFB);
        stateFB = nullptr;
    }

    if (_rb) {
        delete _rb;
        _rb = nullptr;
    }

    SDCARD::close();
}

// -----------------------------------------------------------------------------
// Init
// -----------------------------------------------------------------------------

bool Player::init() {
    _tiles.init();
    _tiles.setTransparent(false);

    if (!SDCARD::init()) {
        _tiles.print("SD init failed", 1, 1, COLOR_RED);
        return false;
    }

    if (!open(_path)) {
        _tiles.print("Failed to open RV file", 1, 1, COLOR_RED);
        return false;
    }

    VGA::clearAll(0);
    return true;
}

// -----------------------------------------------------------------------------
// Update / Tick
// -----------------------------------------------------------------------------

void Player::update(float dt) {

    if (KEYBOARD::isJustPressed(KEYBOARD::ESC)) {
        requestExit();
    }

    if (KEYBOARD::isJustPressed(KEYBOARD::SPACE)) {
        doPause();
    }

    // --- sync audio to video ---
    if (_audio.isPlaying()) {
        _audio.syncToFrame(currentFrame, fps);
        _audio.pause(_isPause);
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
        VGA::show();
    }

    KEYBOARD::beginFrame();
}

void Player::tick() {

}

void Player::doPause() {
    _isPause = !_isPause;
}

// -----------------------------------------------------------------------------
// File open
// -----------------------------------------------------------------------------

bool Player::open(const char* path) {

    LOG.println("Do open");

    if (!SDCARD::open(path)) {
        return false;
    }

    if (SDCARD::getFile() == nullptr) {
        return false;
    }

    _rb = new SdReadBuffer(SDCARD::getFile());

    LOG.println("before read header");

    if (!readHeader()) {
        return false;
    }

    LOG.println("after read header");

    tilesX = width / tileW;
    tilesY = height / tileH;

    currentFrame = 0;
    _isPause = false;

    // --- init audio ---
    if (audioSamples > 0) {
        _audio.init(
            _rb,
            audioOffset,
            audioSamples,
            audioRate
        );
        _audio.start();
    }

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

    width        = readU16(_rb);
    height       = readU16(_rb);
    fps          = readU16(_rb);
    frameCount   = readU32(_rb);
    bpp          = _rb->readU8();
    tileW        = _rb->readU8();
    tileH        = _rb->readU8();
    videoOffset  = readU32(_rb);
    audioOffset  = readU32(_rb);
    audioRate    = readU32(_rb);
    audioSamples = readU32(_rb);

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

    if (height < VGA::height()) {
        _vgaYOffset = (VGA::height() - height) / 2;
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
            VGA::getLinePtr8(y + _vgaYOffset),
            &stateFB[y * width],
            width
        );
    }
}

bool Player::isFinished() const {
    return currentFrame >= frameCount;
}
