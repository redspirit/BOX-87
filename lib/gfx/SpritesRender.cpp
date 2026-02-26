#include "SpritesRender.h"
#include "VGA/VGA.h"
#include <string.h>
#include <stdlib.h>

SpritesRender::SpritesRender() {
    for (int i = 0; i < MAX_SPRITES; i++) {
        sprites_[i].buffer = nullptr;
        sprites_[i].active = false;
    }
}

SpritesRender::~SpritesRender() {
    clear();
}

void SpritesRender::clear() {
    for (int i = 0; i < MAX_SPRITES; i++) {
        if (sprites_[i].active && sprites_[i].buffer) {
            free(sprites_[i].buffer);
        }
        sprites_[i].buffer = nullptr;
        sprites_[i].active = false;
    }
}

int SpritesRender::addSprite(uint8_t* buffer, int x, int y, int w, int h) {
    for (int i = 0; i < MAX_SPRITES; i++) {

        if (!sprites_[i].active) {
            sprites_[i].buffer = buffer;
            sprites_[i].x = x;
            sprites_[i].y = y;
            sprites_[i].w = w;
            sprites_[i].h = h;
            sprites_[i].active = true;
            return i;
        }
    }

    // Нет свободных слотов
    return -1;
}

void SpritesRender::setPosition(int index, int x, int y) {
    if (index < 0 || index >= MAX_SPRITES)
        return;

    if (!sprites_[index].active)
        return;

    sprites_[index].x = x;
    sprites_[index].y = y;
}

void SpritesRender::setPositionY(int index, int y) {
    if (index < 0 || index >= MAX_SPRITES)
        return;

    if (!sprites_[index].active)
        return;

    sprites_[index].y = y;
}

void SpritesRender::removeSprite(int index) {
    if (index < 0 || index >= MAX_SPRITES)
        return;

    if (!sprites_[index].active)
        return;

    if (sprites_[index].buffer)
        free(sprites_[index].buffer);

    sprites_[index].buffer = nullptr;
    sprites_[index].active = false;
}

void SpritesRender::render() {
    for (int i = 0; i < MAX_SPRITES; i++) {
        if (!sprites_[i].active) continue;
        drawSprite(sprites_[i]);
    }
}

void SpritesRender::drawSprite(const Sprite& s) {
    if (!s.buffer) return;

    const int screenW = VGA::width();
    const int screenH = VGA::height();

    // Быстрая проверка: полностью вне экрана
    if (s.x >= screenW || s.y >= screenH)
        return;

    if (s.x + s.w <= 0 || s.y + s.h <= 0)
        return;

    // --- вычисляем область копирования ---

    int srcStartX = 0;
    int srcStartY = 0;
    int dstStartX = s.x;
    int dstStartY = s.y;

    int copyW = s.w;
    int copyH = s.h;

    // LEFT clip
    if (dstStartX < 0) {
        srcStartX = -dstStartX;
        copyW -= srcStartX;
        dstStartX = 0;
    }

    // TOP clip
    if (dstStartY < 0) {
        srcStartY = -dstStartY;
        copyH -= srcStartY;
        dstStartY = 0;
    }

    // RIGHT clip
    if (dstStartX + copyW > screenW) {
        copyW = screenW - dstStartX;
    }

    // BOTTOM clip
    if (dstStartY + copyH > screenH) {
        copyH = screenH - dstStartY;
    }

    // если после клиппинга ничего не осталось
    if (copyW <= 0 || copyH <= 0)
        return;

    // --- рендер ---
    for (int y = 0; y < copyH; y++) {

        uint8_t* dst = VGA::getLinePtr8(dstStartY + y) + dstStartX;

        uint8_t* src =
            s.buffer +
            (srcStartY + y) * s.w +
            srcStartX;

        // если без прозрачности то копируем просто
        memcpy(dst, src, copyW);

        // Пиксельный copy с color-key
        // for (int x = 0; x < copyW; x++) {
        //     uint8_t pixel = src[x];
        //     if (pixel != 0xFF) {
        //         dst[x] = pixel;
        //     }
        // }
    }
}