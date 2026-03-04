#include "editor.h"
#include <cstring>
#include <cstdio>
#include "keyboard.h"
#include "palette.h"
#include "VGA/VGA.h"
#include "sdcard.h"
#include "LOG.h"

Editor::Editor(const char* fullPath) : _tiles() {
    strncpy(_path, fullPath, MAX_PATH);
    _path[MAX_PATH - 1] = 0;
}

Editor::~Editor() {
}

bool Editor::init() {

    paletteInit();
    _tiles.init();

    memset(_buffer, 0, sizeof(_buffer));

    if (!SDCARD::readTextFileLimited(_path, _buffer, MAX_FILE_SIZE)) {
        return false;
    }

    parseLines();
    
    return true;
}

// ============================================================
// Разбор строк (без копирования)
// ============================================================

void Editor::parseLines() {

    _lineCount = 0;

    if (_buffer[0] == '\0') {
        _lineOffsets[_lineCount++] = 0;
        return;
    }

    _lineOffsets[_lineCount++] = 0;

    for (size_t i = 0; _buffer[i] != '\0'; i++) {

        if (_buffer[i] == '\r') {
            _buffer[i] = '\0';
        }

        if (_buffer[i] == '\n') {
            _buffer[i] = '\0';

            if (_lineCount < MAX_LINES) {
                _lineOffsets[_lineCount++] = i + 1;
            }
        }
    }

    if (_lineCount == 0) {
        _lineOffsets[_lineCount++] = 0;
    }
}

// ============================================================
// Ввод
// ============================================================

void Editor::handleInput() {

    if (KEYBOARD::isJustPressed(KEYBOARD::ESC)) {
        requestExit();
    }

    // ===== Навигация =====

    if (KEYBOARD::isJustPressed(KEYBOARD::UP)) {
        if (_cursor.line > 0) {
            _cursor.line--;
        }
    }

    if (KEYBOARD::isJustPressed(KEYBOARD::DOWN)) {
        if (_cursor.line + 1 < _lineCount) {
            _cursor.line++;
        }
    }

    if (KEYBOARD::isJustPressed(KEYBOARD::LEFT)) {

        if (_cursor.column > 0) {
            _cursor.column--;
        }
        else if (_cursor.line > 0) {
            _cursor.line--;
            const char* prevLine = &_buffer[_lineOffsets[_cursor.line]];
            _cursor.column = strlen(prevLine);
        }
    }

    if (KEYBOARD::isJustPressed(KEYBOARD::RIGHT)) {
        const char* line = &_buffer[_lineOffsets[_cursor.line]];
        uint16_t len = strlen(line);

        if (_cursor.column < len) {
            _cursor.column++;
        }
        else if (_cursor.line + 1 < _lineCount) {
            _cursor.line++;
            _cursor.column = 0;
        }
    }

    // ===== Быстрая прокрутка =====

    // if (KEYBOARD::isPressed(KEYBOARD::PAGEUP)) {

    //     uint16_t step = _tiles.gridHeight() - 2;
    //     if (_cursor.line > step)
    //         _cursor.line -= step;
    //     else
    //         _cursor.line = 0;
    // }

    // if (KEYBOARD::isPressed(KEYBOARD::PAGEDOWN)) {

    //     uint16_t step = _tiles.gridHeight() - 2;
    //     if (_cursor.line + step < _lineCount)
    //         _cursor.line += step;
    //     else
    //         _cursor.line = _lineCount - 1;
    // }

    ensureCursorVisible();
}

// ============================================================
// Scroll логика
// ============================================================

void Editor::ensureCursorVisible() {

    const int viewW = _tiles.gridWidth()  - 2;
    const int viewH = _tiles.gridHeight() - 2;

    // Вертикальный scroll
    if (_cursor.line < _scrollY)
        _scrollY = _cursor.line;

    if (_cursor.line >= _scrollY + viewH)
        _scrollY = _cursor.line - viewH + 1;

    // Горизонтальный scroll
    if (_cursor.column < _scrollX)
        _scrollX = _cursor.column;

    if (_cursor.column >= _scrollX + viewW)
        _scrollX = _cursor.column - viewW + 1;
}

// ============================================================
// Рендер
// ============================================================

void Editor::renderEditor() {

    _tiles.clear();

    const int w = _tiles.gridWidth();
    const int h = _tiles.gridHeight();

    uint8_t frameColor  = getColorByPalette(COLOR_CYAN);
    uint8_t textColor   = getColorByPalette(COLOR_WHITE);
    uint8_t statusColor = getColorByPalette(COLOR_YELLOW);

    // ===== Заголовок (в стиле nano) =====

    // Заполняем всю строку пробелами: цвет=frameColor, фон=0, прозрачный=true
    for (int x = 0; x < w; x++) {
        _tiles.drawTile(x, 0, { (uint8_t)' ', frameColor, 0, false, true });
    }

    // Теперь рисуем текст заголовка по центру с инверсией
    // color=frameColor, bgColor=0, isInversion=true
    // При инверсии: пиксели символа=bgColor(0)=чёрный, фон=color(frameColor)
    char title[128];
    snprintf(title, sizeof(title), " BOX87 TEXT EDITOR : %s ", _path);

    int titleLen = strlen(title);
    int titleStartX = (w - titleLen) / 2;
    if (titleStartX < 0) titleStartX = 0;

    for (int i = 0; i < titleLen && titleStartX + i < w; i++) {
        _tiles.drawTile(titleStartX + i, 0, { (uint8_t)title[i], frameColor, 0, true, false });
    }

    // ===== Нижняя рамка =====

    for (int x = 0; x < w; x++) {
        _tiles.drawTile(x, h - 2,    { 0x2500, frameColor, 0, false, true });
    }

    for (int y = 1; y < h - 1; y++) {
        _tiles.drawTile(0,     y, { 0x2502, frameColor, 0, false, true });
        _tiles.drawTile(w - 1, y, { 0x2502, frameColor, 0, false, true });
    }

    // ===== Текст =====

    int viewH = h - 2;
    int viewW = w - 2;

    for (int row = 0; row < viewH; row++) {

        int lineIndex = _scrollY + row;
        if (lineIndex >= _lineCount)
            break;

        const char* line = &_buffer[_lineOffsets[lineIndex]];
        int len = strlen(line);

        for (int col = 0; col < viewW; col++) {

            int charIndex = _scrollX + col;
            if (charIndex >= len)
                break;

            int tileX = col + 1;
            int tileY = row + 1;
            
            // Проверяем, это позиция курсора?
            int cursorX = _cursor.column - _scrollX + 1;
            int cursorY = _cursor.line   - _scrollY + 1;
            
            uint8_t bgColor = (tileX == cursorX && tileY == cursorY) 
                ? getColorByPalette(COLOR_YELLOW) 
                : 0;

            _tiles.drawTile(
                tileX,
                tileY,
                { (uint8_t)line[charIndex], textColor, bgColor, false, false }
            );
        }
    }

    // ===== Статус =====

    char status[128];
    snprintf(status, sizeof(status),
             " %s | Ln:%d Col:%d ",
             _path,
             _cursor.line + 1,
             _cursor.column + 1);

    _tiles.print(status, 1, h - 1, statusColor, 0, false, true);
}

// ============================================================
// Update
// ============================================================

void Editor::update(float dt) {
    handleInput();

    VGA::clear(0);
    renderEditor();
    _tiles.render();
    VGA::show();

    KEYBOARD::beginFrame();
}

void Editor::tick() {
    // Пока ничего не нужно
}