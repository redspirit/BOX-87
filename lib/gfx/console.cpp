#include "console.h"

#include "VGA/VGA.h"
#include <stdlib.h>
#include <string.h>
#include <palette.h>
#include "UTF8.h"

Console::Console()
    : tiles_(), sprites_()
{
    for (int i = 0; i < MAX_IMAGES; i++) {
        inlineImages_[i].active = false;
    }
}

Console::~Console() {
    if (buffer_) {
        free(buffer_);
        buffer_ = nullptr;
    }
}

void Console::init(uint8_t defaultColor) {
    // создаём и инициализируем TextTiles
    tiles_.init();

    width_  = tiles_.gridWidth();
    height_ = tiles_.gridHeight();

    buffer_ = (CharTile*)malloc(width_ * height_ * sizeof(CharTile));
    memset(buffer_, 0, width_ * height_ * sizeof(CharTile));

    defaultColor_ = defaultColor;
    currentColor_ = defaultColor;

    clear();
}

void Console::clear() {
    for (int i = 0; i < height_; i++)
        clearLine(i);

    head_ = 0;
    count_ = 0;
    cx_ = cy_ = 0;
}

void Console::setColor(uint8_t colorIndex) {
    currentColor_ = getColorByPalette(colorIndex);
}

void Console::setColorRaw(uint8_t color) {
    currentColor_ = color;
}

void Console::useDefaultColor() {
    currentColor_ = defaultColor_;
}

inline CharTile& Console::cell(int x, int y) {
    return buffer_[y * width_ + x];
}

void Console::clearLine(int row) {
    for (int x = 0; x < width_; x++) {
        auto& t = cell(x, row);
        t.ch = ' ';
        t.color = defaultColor_;
    }
}

void Console::scrollUp() {
    head_ = (head_ + 1) % height_;
    if (cy_ > 0) cy_--;

    // двигаем все инлайн картинки
    int tileH = tiles_.tileHeight();
    for (int i = 0; i < MAX_IMAGES; i++) {

        if (!inlineImages_[i].active)
            continue;

        inlineImages_[i].yPixels -= tileH;

        sprites_.setPositionY(
            inlineImages_[i].spriteIndex,
            inlineImages_[i].yPixels
        );

        // если полностью ушла вверх
        if (inlineImages_[i].yPixels +
            inlineImages_[i].heightPixels < 0)
        {
            sprites_.removeSprite(
                inlineImages_[i].spriteIndex
            );

            inlineImages_[i].active = false;
        }
    }
}

void Console::newLine() {
    cx_ = 0;

    if (count_ < height_) {
        cy_ = count_;
        clearLine((head_ + count_) % height_);
        count_++;
    } else {
        scrollUp();
        cy_ = height_ - 1;
        clearLine((head_ + cy_) % height_);
    }
}

void Console::printRawChar(uint16_t c, uint16_t repeat) {
    while (repeat--) {
        if (cx_ >= width_)
            newLine();

        int row = (head_ + cy_) % height_;
        auto& t = cell(cx_, row);
        t.ch = c;
        t.color = currentColor_;
        cx_++;
    }
}

void Console::print(const char* text) {
    if (count_ == 0)
        newLine();

    const char* ptr = text;
    while (ptr && *ptr) {
        uint16_t code;
        ptr = UTF8::decode(ptr, code);

        // Обработка переносов строк
        if (code == '\r') {
            cx_ = 0;
            continue;
        }

        if (code == '\n') {
            newLine(); // Переход на новую строку
            continue;
        }

        // Автоматический перенос, если текст вышел за границы ширины
        if (cx_ >= width_) {
            newLine();
        }

        int row = (head_ + cy_) % height_;
        auto& t = cell(cx_, row);
        
        t.ch = code;
        t.color = currentColor_;
        
        cx_++;
    }
}

void Console::printInt(int value) {
    char buf[12];
    itoa(value, buf, 10);
    print(buf);
}

void Console::printLn() {
    newLine();
}

void Console::printLn(const char* text) {
    if (text) print(text);
    newLine();
}

void Console::clearCharAt(int x, int y) {
    if (x < 0 || x >= width_) return;
    if (y < 0 || y >= height_) return;

    int row = (head_ + y) % height_;
    auto& t = cell(x, row);
    t.ch = ' ';
    t.color = defaultColor_;
}

void Console::setCursor(int x, int y) {
    if (x < 0) x = 0;
    if (x >= width_) x = width_ - 1;
    if (y < 0) y = 0;
    if (y >= height_) y = height_ - 1;

    cx_ = x;
    cy_ = y;
}

void Console::getCursor(int& x, int& y) const {
    x = cx_;
    y = cy_;
}

void Console::cursorUpdate(float dt) {
    if (!cursorEnabled_)
        return;

    blinkTimer_ += dt;
    if (blinkTimer_ >= blinkSpeed_) {
        blinkTimer_ = 0.0f;
        cursorPhase_ = !cursorPhase_;
    }
}

void Console::show() {
    show(0, height_ - 1);
    sprites_.render();   // поверх текста
}

void Console::show(int y1, int y2) {
    if (y1 < 0) y1 = 0;
    if (y2 >= height_) y2 = height_ - 1;

    for (int y = y1; y <= y2; y++) {
        int src = (head_ + y) % height_;
        for (int x = 0; x < width_; x++) {
            tiles_.drawTile(x, y, cell(x, src));
        }
    }

    if (cursorEnabled_ && cursorPhase_) {
        tiles_.drawTileForeground(
            cx_, cy_,
            { (uint8_t)cursorChar_, currentColor_, false, false }
        );
        tiles_.foregroundVisible(true);
    } else {
        tiles_.foregroundVisible(false);
    }

    tiles_.render();
}

void Console::cursorSetup(char cursorChar, float cursorBlinkSpeed) {
    cursorChar_ = cursorChar;
    blinkSpeed_ = cursorBlinkSpeed;
}

void Console::setCursorVisible(bool visible) {
    cursorEnabled_ = visible;
    cursorPhase_   = true;     // сразу зажигаем при включении
    blinkTimer_    = 0.0f;
}

bool const Console::getCursorVisible() {
    return cursorEnabled_;
}

int Console::insertImage(uint8_t* buffer, int w, int h) {
    if (!buffer)
        return -1;

    // ищем свободный слот inline image
    int imgSlot = -1;
    for (int i = 0; i < MAX_IMAGES; i++) {
        if (!inlineImages_[i].active) {
            imgSlot = i;
            break;
        }
    }

    if (imgSlot == -1)
        return -1;   // нет свободных слотов

    int tileW = tiles_.tileWidth();
    int tileH = tiles_.tileHeight();

    int px = cx_ * tileW;
    int py = cy_ * tileH;

    int spriteIndex = sprites_.addSprite(buffer, px, py, w, h);

    if (spriteIndex < 0)
        return -1;   // нет слотов спрайтов

    inlineImages_[imgSlot].spriteIndex   = spriteIndex;
    inlineImages_[imgSlot].yPixels       = py;
    inlineImages_[imgSlot].heightPixels  = h;
    inlineImages_[imgSlot].active        = true;

    // --- освобождаем строки под картинку ---
    int rowsNeeded = (h + tileH - 1) / tileH;

    for (int i = 0; i <= rowsNeeded; i++)
        newLine();

    return spriteIndex;
}