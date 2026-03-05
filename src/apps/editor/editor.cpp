#include "editor.h"
#include <cstring>
#include <cstdio>
#include "keyboard.h"
#include "palette.h"
#include "VGA/VGA.h"
#include "sdcard.h"
#include "LOG.h"
#include "UTF8.h"

// ============================================================
// KeyRepeat
// ============================================================

KeyRepeat::KeyRepeat(float initialDelay, float repeatDelay)
    : initialDelay_(initialDelay)
    , repeatDelay_(repeatDelay)
{
}

void KeyRepeat::reset() {
    lastKey_ = 0;
    holdTime_ = 0.0f;
    waitingForRepeat_ = false;
}

bool KeyRepeat::check(uint16_t key, float dt) {
    if (KEYBOARD::isJustPressed(key)) {
        // Новое нажатие
        lastKey_ = key;
        holdTime_ = 0.0f;
        waitingForRepeat_ = true;
        return true;
    }
    
    if (KEYBOARD::isPressed(key) && key == lastKey_) {
        // Клавиша удерживается
        holdTime_ += dt;
        
        if (waitingForRepeat_ && holdTime_ >= initialDelay_) {
            // Первый повтор после initialDelay
            waitingForRepeat_ = false;
            holdTime_ = 0.0f;
            return true;
        }
        
        if (!waitingForRepeat_ && holdTime_ >= repeatDelay_) {
            // Последующие повторы
            holdTime_ = 0.0f;
            return true;
        }
    } else if (key == lastKey_) {
        // Клавиша отпущена
        reset();
    }
    
    return false;
}

Editor::Editor(const char* fullPath) : _tiles(), keyRepeat_(0.5f, 0.05f) {
    strncpy(_path, fullPath, MAX_PATH);
    _path[MAX_PATH - 1] = 0;
}

Editor::~Editor() {
}

bool Editor::init() {

    paletteInit();
    _tiles.init();

    memset(_buffer, 0, sizeof(_buffer));

    // Проверяем, существует ли файл
    if (SDCARD::fileExists(_path)) {
        // Файл существует - загружаем его
        if (!SDCARD::readTextFileLimited(_path, _buffer, MAX_FILE_SIZE)) {
            return false;
        }
        _isModified = false;  // Файл загружен, изменений нет
    } else {
        // Файл не существует - создаём пустой буфер для нового файла
        _buffer[0] = '\0';
        _isModified = true;  // Помечаем как несохранённый
    }

    parseLines();

    return true;
}

// ============================================================
// Разбор строк (без копирования)
// ============================================================

void Editor::parseLines() {
    _lineCount = 0;
    _lineOffsets[_lineCount++] = 0;

    if (_buffer[0] == '\0') return;

    size_t i = 0;
    while (i < MAX_FILE_SIZE - 1 && _buffer[i] != '\0') {
        if (_buffer[i] == '\n' || _buffer[i] == '\r') {
            // Перенос строки ВСЕГДА означает начало новой строки, 
            // даже если она пустая (на ней будет стоять курсор)
            if (_lineCount < MAX_LINES) {
                _lineOffsets[_lineCount++] = i + 1; 
            }
        }
        i++;
    }
}

// ============================================================
// Ввод
// ============================================================

void Editor::handleInput(float dt) {

    // ===== Сначала обрабатываем ввод текста =====
    // Это очищает буфер клавиатуры от старых символов
    
    // BACKSPACE - удаление символа слева
    if (keyRepeat_.check(KEYBOARD::BACKSPACE, dt)) {
        deleteCharBack();
        _isModified = true;
    }

    // DELETE - удаление символа справа
    if (keyRepeat_.check(KEYBOARD::DELETE, dt)) {
        deleteCharForward();
        _isModified = true;
    }

    // ENTER - новая строка
    if (KEYBOARD::isJustPressed(KEYBOARD::ENTER)) {
        insertNewline();
        _isModified = true;
    }

    // ===== Сохранение (Ctrl+S) - ПЕРЕД обработкой символов =====
    bool ctrl = KEYBOARD::isPressed(KEYBOARD::CTRL_LEFT) ||
                KEYBOARD::isPressed(KEYBOARD::CTRL_RIGHT);
    
    if (ctrl && KEYBOARD::isJustPressed(KEYBOARD::S)) {
        if (save()) {
            _isModified = false;
        }
        // Очищаем буфер клавиатуры от 's', чтобы не печаталась
        uint16_t dummy;
        while (KEYBOARD::getChar(_isEngLayout, dummy)) {}
        return;  // Выходим, не обрабатываем остальные клавиши в этом кадре
    }

    // Печатаемые символы - обрабатываем все из буфера
    // (навигационные клавиши уже отфильтрованы в драйвере keyboard.cpp)
    uint16_t ch;
    while (KEYBOARD::getChar(_isEngLayout, ch)) {
        insertChar(ch);
        _isModified = true;
    }

    // ===== Потом навигация =====
    // isJustPressed() будет работать корректно после очистки буфера

    if (keyRepeat_.check(KEYBOARD::ESC, dt)) {
        requestExit();
    }

    if (keyRepeat_.check(KEYBOARD::UP, dt)) {
        if (_cursor.line > 0) {
            _cursor.line--;
            const char* line = &_buffer[_lineOffsets[_cursor.line]];
            uint16_t len = UTF8::length(line);
            if (_cursor.column > len)
                _cursor.column = len;
        }
    }

    if (keyRepeat_.check(KEYBOARD::DOWN, dt)) {
        if (_cursor.line + 1 < _lineCount) {
            _cursor.line++;
            const char* line = &_buffer[_lineOffsets[_cursor.line]];
            uint16_t len = UTF8::length(line);
            if (_cursor.column > len)
                _cursor.column = len;
        }
    }

    if (keyRepeat_.check(KEYBOARD::LEFT, dt)) {
        if (_cursor.column > 0) {
            _cursor.column--;
        }
        else if (_cursor.line > 0) {
            _cursor.line--;
            const char* prevLine = &_buffer[_lineOffsets[_cursor.line]];
            _cursor.column = UTF8::length(prevLine);
        }
    }

    if (keyRepeat_.check(KEYBOARD::RIGHT, dt)) {
        if (_lineCount == 0) {
            // Пустой файл
        } else {
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
    }

    // ===== Быстрая прокрутка =====

    if (keyRepeat_.check(KEYBOARD::PAGEUP, dt)) {
        uint16_t step = _tiles.gridHeight() - 3;
        if (_cursor.line > step) {
            _cursor.line -= step;
        } else {
            _cursor.line = 0;
        }
    }

    if (keyRepeat_.check(KEYBOARD::PAGEDOWN, dt)) {
        uint16_t step = _tiles.gridHeight() - 3;
        if (_cursor.line + step < _lineCount) {
            _cursor.line += step;
        } else {
            _cursor.line = _lineCount - 1;
        }
    }

    // ===== HOME / END =====

    if (keyRepeat_.check(KEYBOARD::HOME, dt)) {
        _cursor.column = 0;
    }

    if (keyRepeat_.check(KEYBOARD::END, dt)) {
        const char* line = &_buffer[_lineOffsets[_cursor.line]];
        _cursor.column = UTF8::length(line);
    }
    
    // ===== Переключение раскладки (Alt+Shift) =====
    bool alt = KEYBOARD::isPressed(KEYBOARD::ALT_LEFT) || 
               KEYBOARD::isPressed(KEYBOARD::ALT_RIGHT);
    bool shift = KEYBOARD::isPressed(KEYBOARD::SHIFT_LEFT) || 
                 KEYBOARD::isPressed(KEYBOARD::SHIFT_RIGHT);
    
    if ((alt && KEYBOARD::isJustPressed(KEYBOARD::SHIFT_LEFT)) ||
        (alt && KEYBOARD::isJustPressed(KEYBOARD::SHIFT_RIGHT)) ||
        (shift && KEYBOARD::isJustPressed(KEYBOARD::ALT_LEFT)) ||
        (shift && KEYBOARD::isJustPressed(KEYBOARD::ALT_RIGHT))) {
        _isEngLayout = !_isEngLayout;
    }

    // ===== Ограничение позиции курсора пределами строки =====
    if (_lineCount > 0 && _cursor.line < _lineCount) {
        const char* line = &_buffer[_lineOffsets[_cursor.line]];
        uint16_t len = 0;
        const char* ptr = line;
        while (*ptr && *ptr != '\n' && *ptr != '\r') {
            uint16_t code;
            ptr = UTF8::decode(ptr, code);
            len++;
        }
        if (_cursor.column > len)
            _cursor.column = len;
    }

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

    // Боковые границы (до нижней рамки)
    for (int y = 1; y < h; y++) {
        _tiles.drawTile(0,     y, { 0x2502, frameColor, 0, false, true });
        _tiles.drawTile(w - 1, y, { 0x2502, frameColor, 0, false, true });
    }

    // Углы
    _tiles.drawTile(0,     h - 2, { 0x251C, frameColor, 0, false, true });  // левый-нижний
    _tiles.drawTile(w - 1, h - 2, { 0x2524, frameColor, 0, false, true });  // правый-нижний

    // ===== Заголовок =====

    // Рисуем текст заголовка по центру с инверсией
    char title[128];
    if (_isModified) {
        snprintf(title, sizeof(title), " BOX87 TEXT EDITOR : %s [*] ", _path);
    } else {
        snprintf(title, sizeof(title), " BOX87 TEXT EDITOR : %s ", _path);
    }

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

    // Защита от пустого файла
    if (_lineCount == 0) {
        _lineCount = 1;
        _lineOffsets[0] = 0;
        _buffer[0] = '\0';
    }

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

            // Конец строки (теперь ловим и \n, и \r, и \0)
            if (code == 0 || code == '\n' || code == '\r') {
                // Рисуем пробелы до конца визуальной строки
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

    // Подсчитываем общее количество символов (Unicode) и байт
    uint32_t totalChars = 0;
    uint32_t totalBytes = 0;

    for (uint16_t i = 0; i < _lineCount; i++) {
        const char* line = &_buffer[_lineOffsets[i]];
        
        // Считаем длину строки до \n или \0
        uint16_t lineByteLen = 0;
        uint16_t lineCharLen = 0;
        const char* ptr = line;
        
        while (*ptr && *ptr != '\n' && *ptr != '\r') {
            uint16_t code;
            ptr = UTF8::decode(ptr, code);
            lineByteLen += (code < 0x80) ? 1 : (code < 0x800) ? 2 : 3;
            lineCharLen++;
        }
        
        totalBytes += lineByteLen;
        totalChars += lineCharLen;

        // Добавляем символ newline (кроме последней строки)
        if (i < _lineCount - 1) {
            totalBytes++;    // 1 байт для \n
            totalChars++;    // 1 символ newline
        }
    }

    // Левая часть статус-бара
    char statusLeft[64];
    snprintf(statusLeft, sizeof(statusLeft),
             " Ln:%d Col:%d / %lu bytes / %lu chars ",
             _cursor.line + 1,
             _cursor.column + 1,
             totalBytes,
             totalChars);

    // Правая часть статус-бара с индикатором раскладки
    char statusRight[48];
    snprintf(statusRight, sizeof(statusRight),
             "Exit-Esc Save-^S [%s]",
             _isEngLayout ? "EN" : "RU");

    // Вычисляем позицию для правой части
    int statusWidth = w - 2;
    int leftLen = strlen(statusLeft);
    int rightLen = strlen(statusRight);
    int rightStartX = statusWidth - rightLen;

    // Рисуем левую часть
    _tiles.print(statusLeft, 1, h - 1, statusColor, 0, false, true);

    // Рисуем правую часть
    if (rightStartX > leftLen) {
        _tiles.print(statusRight, rightStartX, h - 1, statusColor, 0, false, true);
    }
}

// ============================================================
// Update
// ============================================================

void Editor::update(float dt) {
    handleInput(dt);

    VGA::clear(0);
    renderEditor();
    _tiles.render();
    VGA::show();

    KEYBOARD::beginFrame();
}

// ============================================================
// Вставка/удаление символов
// ============================================================

// Вставка UTF-8 символа на позицию курсора
void Editor::insertChar(uint16_t ch) {
    // Проверка границ
    if (_lineCount == 0 || _cursor.line >= _lineCount)
        return;
    
    // Проверка на допустимый символ (не управляющий)
    if (ch < 0x20 || ch == 0x7F)
        return;
    
    // Кодируем Unicode в UTF-8
    char utf8[4];
    int utf8Len = 0;

    if (ch < 0x80) {
        utf8[0] = (char)ch;
        utf8Len = 1;
    } else if (ch < 0x800) {
        utf8[0] = (char)(0xC0 | (ch >> 6));
        utf8[1] = (char)(0x80 | (ch & 0x3F));
        utf8Len = 2;
    } else {
        utf8[0] = (char)(0xE0 | (ch >> 12));
        utf8[1] = (char)(0x80 | ((ch >> 6) & 0x3F));
        utf8[2] = (char)(0x80 | (ch & 0x3F));
        utf8Len = 3;
    }

    // Находим текущую строку и позицию вставки в байтах
    char* currentLine = &_buffer[_lineOffsets[_cursor.line]];
    int bytePos = 0;
    int col = 0;
    const char* ptr = currentLine;

    // Ищем позицию с учетом того, что строка заканчивается на \n или \0
    while (col < _cursor.column && *ptr && *ptr != '\n' && *ptr != '\r') {
        uint16_t code;
        ptr = UTF8::decode(ptr, code);
        bytePos += (code < 0x80) ? 1 : (code < 0x800) ? 2 : 3;
        col++;
    }

    // Проверяем, есть ли место в буфере
    int totalLen = strlen(_buffer);
    if (totalLen + utf8Len >= MAX_FILE_SIZE - 2) {
        return;  // Буфер полон
    }

    // ГЛОБАЛЬНЫЙ индекс вставки относительно начала _buffer
    int absoluteInsertPos = _lineOffsets[_cursor.line] + bytePos;

    // Сдвигаем ВЕСЬ остаток файла вправо (+1 для переноса нуль-терминатора)
    memmove(&_buffer[absoluteInsertPos + utf8Len], 
            &_buffer[absoluteInsertPos], 
            totalLen - absoluteInsertPos + 1);

    // Вставляем байты символа
    memcpy(&_buffer[absoluteInsertPos], utf8, utf8Len);

    // Перестраиваем offsets строк
    parseLines();

    // Двигаем курсор вправо
    _cursor.column++;
}

// Вставка новой строки (ENTER)
void Editor::insertNewline() {
    if (_lineCount == 0 || _cursor.line >= _lineCount)
        return;
    
    char* line = &_buffer[_lineOffsets[_cursor.line]];
    
    // Находим позицию разрыва в байтах
    int bytePos = 0;
    int col = 0;
    const char* ptr = line;
    
    while (col < _cursor.column && *ptr && *ptr != '\n' && *ptr != '\r') {
        uint16_t code;
        ptr = UTF8::decode(ptr, code);
        bytePos += (code < 0x80) ? 1 : (code < 0x800) ? 2 : 3;
        col++;
    }

    // Проверяем место в буфере (нужен 1 байт для \n)
    int totalLen = strlen(_buffer);
    if (totalLen + 1 >= MAX_FILE_SIZE - 1) {
        return;  // Буфер полон
    }

    // ГЛОБАЛЬНЫЙ индекс вставки
    int absoluteInsertPos = _lineOffsets[_cursor.line] + bytePos;

    // Сдвигаем ВЕСЬ остаток файла вправо
    memmove(&_buffer[absoluteInsertPos + 1], 
            &_buffer[absoluteInsertPos], 
            totalLen - absoluteInsertPos + 1);

    // Вставляем \n
    _buffer[absoluteInsertPos] = '\n';

    // Перестраиваем offsets строк
    parseLines();

    // Двигаем курсор на новую строку
    _cursor.line++;
    _cursor.column = 0;
}

// Удаление символа слева (BACKSPACE)
void Editor::deleteCharBack() {
    if (_cursor.column > 0) {
        // Удаляем символ слева
        char* line = &_buffer[_lineOffsets[_cursor.line]];

        // Находим позицию символа для удаления в байтах
        int bytePos = 0;
        int col = 0;
        const char* ptr = line;
        int prevBytePos = 0;

        while (col < _cursor.column && *ptr && *ptr != '\n' && *ptr != '\r') {
            uint16_t code;
            prevBytePos = bytePos;
            ptr = UTF8::decode(ptr, code);
            bytePos += (code < 0x80) ? 1 : (code < 0x800) ? 2 : 3;
            col++;
        }

        // Вычисляем длину удаляемого символа
        int charLen = bytePos - prevBytePos;

        // ГЛОБАЛЬНЫЙ индекс удаления
        int absoluteDeletePos = _lineOffsets[_cursor.line] + prevBytePos;

        // Сдвигаем ВЕСЬ остаток файла влево
        // Длина от источника + 1 для захвата \0
        memmove(&_buffer[absoluteDeletePos],
                &_buffer[absoluteDeletePos + charLen],
                strlen(&_buffer[absoluteDeletePos + charLen]) + 1);

        // Перестраиваем offsets
        parseLines();

        // Двигаем курсор влево
        _cursor.column--;
    } else if (_cursor.line > 0) {
        // Курсор в начале строки - объединяем с предыдущей
        // Находим длину предыдущей строки (до \n)
        char* prevLine = &_buffer[_lineOffsets[_cursor.line - 1]];
        int prevLineLen = 0;
        while (prevLine[prevLineLen] && prevLine[prevLineLen] != '\n' && prevLine[prevLineLen] != '\r') {
            prevLineLen++;
        }
        
        // ГЛОБАЛЬНЫЙ индекс \n который нужно удалить
        int newlinePos = _lineOffsets[_cursor.line - 1] + prevLineLen;

        // Сдвигаем ВЕСЬ остаток файла влево на 1 (удаляем \n)
        // Длина от источника + 1 для захвата \0
        memmove(&_buffer[newlinePos],
                &_buffer[newlinePos + 1],
                strlen(&_buffer[newlinePos + 1]) + 1);

        // Перестраиваем offsets
        parseLines();

        // Двигаем курсор на предыдущую строку
        _cursor.line--;
        _cursor.column = prevLineLen;
    }
}

// Удаление символа справа (DELETE)
void Editor::deleteCharForward() {
    if (_lineCount == 0 || _cursor.line >= _lineCount)
        return;
    
    char* line = &_buffer[_lineOffsets[_cursor.line]];
    int lineLen = 0;
    while (line[lineLen] && line[lineLen] != '\n' && line[lineLen] != '\r') {
        lineLen++;
    }

    // Если курсор в конце строки - объединяем со следующей
    if (_cursor.column >= (int)UTF8::length(line)) {
        if (_cursor.line + 1 < _lineCount) {
            // ГЛОБАЛЬНЫЙ индекс \n который нужно удалить
            int newlinePos = _lineOffsets[_cursor.line] + lineLen;

            // Сдвигаем ВЕСЬ остаток файла влево на 1 (удаляем \n)
            // Длина от источника + 1 для захвата \0
            memmove(&_buffer[newlinePos],
                    &_buffer[newlinePos + 1],
                    strlen(&_buffer[newlinePos + 1]) + 1);

            // Перестраиваем offsets
            parseLines();
        }
        return;
    }

    // Находим позицию символа для удаления в байтах
    int bytePos = 0;
    int col = 0;
    const char* ptr = line;

    while (col < _cursor.column && *ptr && *ptr != '\n' && *ptr != '\r') {
        uint16_t code;
        ptr = UTF8::decode(ptr, code);
        bytePos += (code < 0x80) ? 1 : (code < 0x800) ? 2 : 3;
        col++;
    }

    // Вычисляем длину удаляемого символа
    uint16_t code;
    const char* charPtr = line + bytePos;
    UTF8::decode(charPtr, code);
    int charLen = (code < 0x80) ? 1 : (code < 0x800) ? 2 : 3;

    // ГЛОБАЛЬНЫЙ индекс удаления
    int absoluteDeletePos = _lineOffsets[_cursor.line] + bytePos;

    // Сдвигаем ВЕСЬ остаток файла влево
    // Длина от источника + 1 для захвата \0
    memmove(&_buffer[absoluteDeletePos],
            &_buffer[absoluteDeletePos + charLen],
            strlen(&_buffer[absoluteDeletePos + charLen]) + 1);

    // Перестраиваем offsets
    parseLines();
}

// ============================================================
// Сохранение файла
// ============================================================

bool Editor::save() {
    // Весь текст уже лежит в _buffer в правильном формате (с \n внутри),
    // поэтому нам достаточно узнать длину всей строки до финального \0.
    size_t totalLen = strlen(_buffer);
    
    bool result = false;
    if (SDCARD::open(_path, "w")) {
        File* f = SDCARD::getFile();
        if (f && f->write((const uint8_t*)_buffer, totalLen) == totalLen) {
            result = true;
        }
        SDCARD::close();
    }
    
    if (result) {
        LOG.print("File saved: ");
        LOG.println(_path);
    } else {
        LOG.println("Write failed");
    }
    
    return result;
}

void Editor::tick() {
    // Пока ничего не нужно
}