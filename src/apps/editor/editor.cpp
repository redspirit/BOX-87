#include "editor.h"
#include <cstring>
#include <cstdio>
#include "keyboard.h"
#include "palette.h"
#include "VGA/VGA.h"
#include "sdcard.h"
#include "LOG.h"
#include "UTF8.h"

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
            // Ограничиваем позицию курсора длиной строки (в Unicode символах)
            const char* line = &_buffer[_lineOffsets[_cursor.line]];
            uint16_t len = UTF8::length(line);
            if (_cursor.column > len)
                _cursor.column = len;
        }
    }

    if (KEYBOARD::isJustPressed(KEYBOARD::DOWN)) {
        if (_cursor.line + 1 < _lineCount) {
            _cursor.line++;
            // Ограничиваем позицию курсора длиной строки (в Unicode символах)
            const char* line = &_buffer[_lineOffsets[_cursor.line]];
            uint16_t len = UTF8::length(line);
            if (_cursor.column > len)
                _cursor.column = len;
        }
    }

    if (KEYBOARD::isJustPressed(KEYBOARD::LEFT)) {
        if (_cursor.column > 0) {
            _cursor.column--;
        }
        else if (_cursor.line > 0) {
            _cursor.line--;
            const char* prevLine = &_buffer[_lineOffsets[_cursor.line]];
            _cursor.column = UTF8::length(prevLine);
        }
    }

    if (KEYBOARD::isJustPressed(KEYBOARD::RIGHT)) {
        const char* line = &_buffer[_lineOffsets[_cursor.line]];
        uint16_t len = UTF8::length(line);

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
    const int viewH = _tiles.gridHeight() - 3;  // Как в renderEditor()

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

    // ===== Рамка =====

    // Верхняя граница
    for (int x = 1; x < w - 1; x++) {
        _tiles.drawTile(x, 0, { 0x2550, frameColor, 0, false, true });
    }
    // Углы
    _tiles.drawTile(0,     0, { 0x2552, frameColor, 0, false, true });  // левый-верхний
    _tiles.drawTile(w - 1, 0, { 0x2555, frameColor, 0, false, true });  // правый-верхний

    // Нижняя граница (выше статус-бара)
    for (int x = 1; x < w - 1; x++) {
        _tiles.drawTile(x, h - 2, { 0x2500, frameColor, 0, false, true });
    }

    // Боковые границы
    for (int y = 1; y < h - 2; y++) {
        _tiles.drawTile(0,     y, { 0x2502, frameColor, 0, false, true });
        _tiles.drawTile(w - 1, y, { 0x2502, frameColor, 0, false, true });
    }

    // Углы
    _tiles.drawTile(0,     h - 2, { 0x251C, frameColor, 0, false, true });  // левый-нижний
    _tiles.drawTile(w - 1, h - 2, { 0x2524, frameColor, 0, false, true });  // правый-нижний

    // ===== Заголовок =====

    // Рисуем текст заголовка по центру с инверсией
    char title[128];
    snprintf(title, sizeof(title), " BOX87 TEXT EDITOR : %s ", _path);

    int titleLen = strlen(title);
    int titleStartX = (w - titleLen) / 2;
    if (titleStartX < 0) titleStartX = 0;

    // Рисуем текст заголовка с инверсией
    for (int i = 0; i < titleLen && titleStartX + i < w; i++) {
        _tiles.drawTile(titleStartX + i, 0, { (uint16_t)title[i], frameColor, 0, true, false });
    }

    // ===== Текст =====

    int viewH = h - 3;  // Не доходим до нижней рамки
    int viewW = w - 2;

    for (int row = 0; row < viewH; row++) {

        int lineIndex = _scrollY + row;
        if (lineIndex >= _lineCount)
            break;

        const char* line = &_buffer[_lineOffsets[lineIndex]];
        
        // Вычисляем позицию курсора
        int cursorX = _cursor.column - _scrollX + 1;
        int cursorY = _cursor.line   - _scrollY + 1;

        // Декодируем UTF-8 символы и рисуем их
        const char* ptr = line;
        int col = 0;  // визуальная колонка (Unicode символы)
        
        while (col < viewW + _scrollX) {
            uint16_t code;
            ptr = UTF8::decode(ptr, code);
            
            // Конец строки
            if (code == 0) {
                // Рисуем пробелы до конца строки
                for (int restCol = col; restCol < viewW + _scrollX; restCol++) {
                    int tileX = restCol - _scrollX + 1;
                    int tileY = row + 1;
                    
                    bool isCursor = (tileX == cursorX && tileY == cursorY);
                    uint8_t bgColor = isCursor ? getColorByPalette(COLOR_YELLOW) : 0;
                    uint8_t charColor = isCursor ? 0 : textColor;
                    
                    _tiles.drawTile(tileX, tileY, { (uint16_t)' ', charColor, bgColor, false, false });
                }
                break;
            }
            
            // Рисуем символ, если он в видимой области
            if (col >= _scrollX) {
                int tileX = col - _scrollX + 1;
                int tileY = row + 1;
                
                bool isCursor = (tileX == cursorX && tileY == cursorY);
                uint8_t bgColor = isCursor ? getColorByPalette(COLOR_YELLOW) : 0;
                uint8_t charColor = isCursor ? 0 : textColor;
                
                _tiles.drawTile(tileX, tileY, { code, charColor, bgColor, false, false });
            }
            
            col++;
        }
    }

    // ===== Scrollbar (вертикальный) =====

    int scrollStartY = 1;
    int scrollEndY = h - 3;
    int scrollTrackHeight = scrollEndY - scrollStartY + 1;

    // Вычисляем высоту "бегунка"
    int thumbHeight = (_lineCount > 0) ? (viewH * scrollTrackHeight) / _lineCount : 1;
    if (thumbHeight < 1) thumbHeight = 1;
    if (thumbHeight > scrollTrackHeight) thumbHeight = scrollTrackHeight;

    // Вычисляем позицию "бегунка"
    int scrollableLines = _lineCount - viewH;
    int thumbY = (scrollableLines > 0)
        ? scrollStartY + (_scrollY * (scrollTrackHeight - thumbHeight)) / scrollableLines
        : scrollStartY;

    // Рисуем только бегунок scrollbar поверх правой границы
    for (int y = thumbY; y < thumbY + thumbHeight && y <= scrollEndY; y++) {
        _tiles.drawTile(w - 1, y, {
            0x2592,
            frameColor,
            0,
            false,
            false
        });
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