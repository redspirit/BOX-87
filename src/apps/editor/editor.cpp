#include "editor.h"
#include <cstring>
#include <cstdio>
#include "keyboard.h"
#include "palette.h"
#include "VGA/VGA.h"
#include "sdcard.h"
#include "UTF8.h"

// ============================================================
// Callback функции для Lua (stdout/stderr)
// ============================================================

static void scrollLuaOutput(Editor* editor)
{
    TextTiles& tiles = editor->tiles();

    int width  = tiles.gridWidth();
    int height = tiles.gridHeight();

    // Сдвигаем все строки вверх
    for (int y = 1; y < height - 1; y++)
    {
        for (int x = 1; x < width - 1; x++)
        {
            auto t = tiles.getTile(x, y + 1);
            tiles.drawTile(x, y, t);
        }
    }

    // Очищаем последнюю строку
    for (int x = 1; x < width - 1; x++)
    {
        tiles.drawTile(x, height - 2, { ' ', 0, 0, false, false });
    }

    editor->luaPrintY() = height - 2;
}

static void printLuaStream(Editor* editor, const char* text, uint8_t color)
{
    if (!editor || !text) return;

    TextTiles& tiles = editor->tiles();

    uint16_t& x = editor->luaPrintX();
    uint16_t& y = editor->luaPrintY();

    int width  = tiles.gridWidth();
    int height = tiles.gridHeight();

    if (x == 1 && y == 1)
    {
        VGA::clear(0);
        tiles.clear();
    }

    const char* p = text;

    while (*p)
    {
        char c = *p++;

        if (c == '\r')
            continue;

        if (c == '\n')
        {
            x = 1;
            y++;

            if (y >= height - 1)
                scrollLuaOutput(editor);

            continue;
        }

        tiles.drawTile(x, y, {
            (uint16_t)c,
            color,
            0,
            false,
            false
        });

        x++;

        if (x >= width - 1)
        {
            x = 1;
            y++;

            if (y >= height - 1)
                scrollLuaOutput(editor);
        }
    }

    tiles.render();
    VGA::show();

    yield();
}

static void lua_stdout_callback(const char* text, void* userData)
{
    Editor* editor = static_cast<Editor*>(userData);

    printLuaStream(
        editor,
        text,
        getColorByPalette(COLOR_WHITE)
    );
}

static void lua_stderr_callback(const char* text, void* userData)
{
    Editor* editor = static_cast<Editor*>(userData);

    printLuaStream(
        editor,
        text,
        getColorByPalette(COLOR_RED)
    );
}

// Reader callback для загрузки из памяти
static size_t lua_memory_reader(uint8_t* buffer, size_t maxSize, void* userData) {
    Editor* editor = static_cast<Editor*>(userData);
    if (!editor) return 0;

    static size_t pos = 0;

    const char* src = editor->buffer();
    size_t total = strlen(src);

    if (pos >= total)
    {
        pos = 0;
        return 0;
    }

    size_t remaining = total - pos;
    size_t toRead = remaining;

    if (toRead > maxSize)
        toRead = maxSize;

    memcpy(buffer, src + pos, toRead);
    pos += toRead;

    return toRead;
}

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
    
    // Инициализируем NULL указатели и переменные
    _luaRunner = nullptr;
    _luaPrintY = 1;
    _state = EditorState::STATE_EDIT;
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

    // Инициализируем профиль подсветки синтаксиса
    _syntaxProfile = getProfileForFile(_path);

    return true;
}

// ============================================================
// Запуск/остановка Lua кода
// ============================================================

void Editor::runLua() {
    // Проверяем расширение файла
    const char* ext = strrchr(_path, '.');
    if (!ext || strcmp(ext, ".lua") != 0) {
        return;  // Не lua файл
    }
    
    // Очищаем предыдущий runner если есть
    if (_luaRunner) {
        delete _luaRunner;
        _luaRunner = nullptr;
    }
    
    // Сбрасываем позицию печати
    _luaPrintY = 1;
    _luaPrintX = 1;
    
    // Создаём runner
    _luaRunner = new LuaRunner();
    if (!_luaRunner) {
        return;  // Не удалось выделить память
    }
    
    // Инициализируем Lua state
    if (!_luaRunner->init()) {
        delete _luaRunner;
        _luaRunner = nullptr;
        return;
    }
    
    // Переключаем состояние ПЕРЕД запуском
    _state = EditorState::STATE_RUNNING;
    
    // Сбрасываем позицию печати
    _luaPrintY = 1;
    
    // Очищаем экран ЧЁРНЫМ
    VGA::clear(0);
    _tiles.clear();
    VGA::show();
    
    // Запускаем код из буфера (выполняется синхронно)
    bool result = _luaRunner->run(lua_memory_reader, this, lua_stdout_callback, lua_stderr_callback, this);
    
    if (!result) {
        // Ошибка загрузки
        delete _luaRunner;
        _luaRunner = nullptr;
        _state = EditorState::STATE_EDIT;
        return;
    }
    
    printLuaStream(
        this,
        "\n[Program finished]\n",
        getColorByPalette(COLOR_GREEN)
    );
    printLuaStream(
        this,
        "Press any key...\n",
        getColorByPalette(COLOR_GRAY)
    );
    
    // Теперь переходим в FINISHED и ждём любую клавишу
    _state = EditorState::STATE_FINISHED;
}

void Editor::stopLua() {
    // Освобождаем ресурсы
    if (_luaRunner) {
        delete _luaRunner;
        _luaRunner = nullptr;
    }
    
    // Сбрасываем позицию печати
    _luaPrintY = 1;
    
    // Переключаем состояние
    _state = EditorState::STATE_FINISHED;
    
    // Очищаем экран
    VGA::clear(0);
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

    // ===== Обработка состояний =====
    
    if (_state == EditorState::STATE_FINISHED) {
        // Ждём ЛЮБУЮ клавишу для возврата в редактор
        // Проверяем все популярные клавиши
        bool anyKey = 
            KEYBOARD::isJustPressed(KEYBOARD::ESC) ||
            KEYBOARD::isJustPressed(KEYBOARD::ENTER) ||
            KEYBOARD::isJustPressed(KEYBOARD::SPACE) ||
            KEYBOARD::isJustPressed(KEYBOARD::UP) ||
            KEYBOARD::isJustPressed(KEYBOARD::DOWN) ||
            KEYBOARD::isJustPressed(KEYBOARD::LEFT) ||
            KEYBOARD::isJustPressed(KEYBOARD::RIGHT);
        
        if (anyKey) {
            KEYBOARD::flush();
            keyRepeat_.reset();
            // Возвращаемся в режим редактирования
            _state = EditorState::STATE_EDIT;
        }
        return;
    }
    
    // ===== STATE_EDIT - обычное редактирование =====

    // F5 - запуск Lua кода
    if (KEYBOARD::isJustPressed(KEYBOARD::F5)) {
        runLua();
        return;  // Выход, не обрабатывать остальное
    }

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
        KEYBOARD::flush();
        return;  // Выходим, не обрабатываем остальные клавиши в этом кадре
    }
    
    // Ctrl+R - удалить текущую строку
    if (ctrl && KEYBOARD::isJustPressed(KEYBOARD::R)) {
        deleteCurrentLine();
        _isModified = true;
        // Очищаем буфер от 'r'
        KEYBOARD::flush();
        return;
    }
    
    // Ctrl+D - дублировать текущую строку
    if (ctrl && KEYBOARD::isJustPressed(KEYBOARD::D)) {
        duplicateCurrentLine();
        _isModified = true;
        // Очищаем буфер от 'd'
        KEYBOARD::flush();
        return;
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
// Подсветка синтаксиса
// ============================================================

// Анализирует строку и возвращает массив цветов для каждого символа
// Возвращает количество проанализированных байт
static int analyzeLine(
    const LanguageProfile* profile,
    const char* line,
    uint8_t* colors,
    int maxLen,
    uint8_t defaultColor
) {
    if (!profile || !profile->rules) {
        return 0;
    }
    
    int pos = 0;
    
    while (pos < maxLen && line[pos] != '\0' && line[pos] != '\n' && line[pos] != '\r') {
        bool matched = false;
        const char* remaining = &line[pos];
        
        // Проверяем все правила
        for (size_t r = 0; r < profile->ruleCount; r++) {
            const SyntaxRule& rule = profile->rules[r];
            size_t startLen = strlen(rule.start);
            if (startLen == 0) continue;
            
            // ===== 1. ДИАПАЗОНЫ (Строки, Комментарии) =====
            if (rule.type == RuleType::RANGE) {
                if (strncmp(remaining, rule.start, startLen) == 0) {
                    size_t endLen = strlen(rule.end);
                    uint8_t ruleColor = getColorByPalette(rule.color);  // Конвертируем цвет

                    for (size_t i = 0; i < startLen && pos < maxLen; i++) {
                        colors[pos++] = ruleColor;
                    }

                    while (pos < maxLen && line[pos] != '\0' && line[pos] != '\n' && line[pos] != '\r') {
                        if (endLen > 0 && strncmp(&line[pos], rule.end, endLen) == 0) {
                            for (size_t i = 0; i < endLen && pos < maxLen; i++) {
                                colors[pos++] = ruleColor;
                            }
                            break;
                        }
                        colors[pos++] = ruleColor;
                    }
                    matched = true;
                    break;
                }
            }
            // ===== 2. ПОСЛЕДОВАТЕЛЬНОСТИ (Скобки, Операторы) =====
            else if (rule.type == RuleType::SEQUENCE) {
                if (strncmp(remaining, rule.start, startLen) == 0) {
                    uint8_t ruleColor = getColorByPalette(rule.color);  // Конвертируем цвет
                    for (size_t i = 0; i < startLen && pos < maxLen; i++) {
                        colors[pos++] = ruleColor;
                    }
                    matched = true;
                    break;
                }
            }
            // ===== 3. КЛЮЧЕВЫЕ СЛОВА =====
            else if (rule.type == RuleType::KEYWORD) {
                bool leftBoundaryOK = (pos == 0 || !isWordChar(line[pos - 1]));
                bool rightBoundaryOK = !isWordChar(line[pos + startLen]);

                if (leftBoundaryOK && rightBoundaryOK && strncmp(remaining, rule.start, startLen) == 0) {
                    uint8_t ruleColor = getColorByPalette(rule.color);  // Конвертируем цвет
                    for (size_t i = 0; i < startLen && pos < maxLen; i++) {
                        colors[pos++] = ruleColor;
                    }
                    matched = true;
                    break;
                }
            }
        }
        
        // ===== 4. ЧИСЛА (Автоматическая подсветка) =====
        // Если ни одно правило не сработало, и мы видим цифру
        if (!matched && remaining[0] >= '0' && remaining[0] <= '9') {
            
            // Убеждаемся, что цифра не является частью слова (например, var1)
            bool isPartOfWord = false;
            if (pos > 0) {
                char prev = line[pos - 1];
                if ((prev >= 'a' && prev <= 'z') || (prev >= 'A' && prev <= 'Z') || prev == '_') {
                    isPartOfWord = true;
                }
            }
            
            if (!isPartOfWord) {
                // Красим всё, что похоже на число
                while (pos < maxLen && line[pos] != '\0' && line[pos] != '\n' && line[pos] != '\r') {
                    char c = line[pos];
                    // Разрешаем цифры, точку (для дробей 3.14), и символы x, a-f (для 0xFF)
                    if ((c >= '0' && c <= '9') || c == '.' || c == 'x' || c == 'X' || 
                        (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
                        colors[pos++] = getColorByPalette(SYNTAX_COLOR_NUMBER);
                    } else {
                        break; // Число закончилось
                    }
                }
                matched = true;
            }
        }
        
        // ===== 5. ДЕФОЛТНЫЙ ЦВЕТ =====
        // Если вообще ничего не подошло
        if (!matched) {
            colors[pos++] = defaultColor;
        }
    }
    
    return pos;
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

    // Буфер для цветов синтаксиса (максимальная длина строки)
    static uint8_t syntaxColors[1024];  // Статический буфер для экономии стека

    for (int row = 0; row < viewH; row++) {

        int lineIndex = _scrollY + row;
        if (lineIndex >= _lineCount)
            break;

        const char* line = &_buffer[_lineOffsets[lineIndex]];
        
        // Анализируем синтаксис строки
        uint8_t defaultColor = textColor;
        int analyzedLen = 0;
        
        if (_syntaxProfile && _syntaxProfile->rules) {
            analyzedLen = analyzeLine(
                _syntaxProfile,
                line,
                syntaxColors,
                1023,  // Максимум символов в строке
                defaultColor
            );
        }

        // Вычисляем позицию курсора
        int cursorX = _cursor.column - _scrollX + 1;
        int cursorY = _cursor.line   - _scrollY + 1;

        // Декодируем UTF-8 символы и рисуем их
        const char* ptr = line;
        int col = 0;  // визуальная колонка (Unicode символы)
        int bytePos = 0;  // позиция в байтах

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
                    uint8_t charColor = isCursor ? 0 : defaultColor;

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
                
                // Получаем цвет из анализа синтаксиса
                uint8_t charColor = defaultColor;
                if (bytePos < analyzedLen && _syntaxProfile && _syntaxProfile->rules) {
                    charColor = syntaxColors[bytePos];
                }
                
                // Курсор перекрывает цвет
                if (isCursor) {
                    charColor = 0;  // Чёрный текст на жёлтом фоне
                }

                _tiles.drawTile(tileX, tileY, { code, charColor, bgColor, false, false });
            }

            // Вычисляем длину символа в байтах
            int symbolLen = (code < 0x80) ? 1 : (code < 0x800) ? 2 : 3;
            bytePos += symbolLen;
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
// Удаление/дублирование строки
// ============================================================

// Удаление текущей строки
void Editor::deleteCurrentLine() {
    if (_lineCount == 0 || _cursor.line >= _lineCount)
        return;
    
    // Находим длину текущей строки в байтах
    char* line = &_buffer[_lineOffsets[_cursor.line]];
    int lineLen = 0;
    while (line[lineLen] && line[lineLen] != '\n' && line[lineLen] != '\r') {
        lineLen++;
    }
    
    // Глобальная позиция начала строки
    int lineStartPos = _lineOffsets[_cursor.line];
    
    // Проверяем, есть ли \n после строки (не последняя ли строка)
    int bytesToDelete = lineLen;
    if (line[lineLen] == '\n' || line[lineLen] == '\r') {
        bytesToDelete++;  // Удаляем и \n
    }
    
    // Сдвигаем весь остаток файла влево
    memmove(&_buffer[lineStartPos],
            &_buffer[lineStartPos + bytesToDelete],
            strlen(&_buffer[lineStartPos + bytesToDelete]) + 1);
    
    // Если курсор был на последней строке, перемещаем на предыдущую
    if (_cursor.line >= _lineCount - 1 && _cursor.line > 0) {
        _cursor.line--;
    }
    
    // Сбрасываем колонку в 0
    _cursor.column = 0;
    
    // Перестраиваем offsets
    parseLines();
}

// Дублирование текущей строки
void Editor::duplicateCurrentLine() {
    if (_lineCount == 0 || _cursor.line >= _lineCount)
        return;
    
    // Находим длину текущей строки в байтах
    char* line = &_buffer[_lineOffsets[_cursor.line]];
    int lineLen = 0;
    while (line[lineLen] && line[lineLen] != '\n' && line[lineLen] != '\r') {
        lineLen++;
    }
    
    // Проверяем место в буфере (нужно lineLen + 1 для \n)
    int totalLen = strlen(_buffer);
    int insertSize = lineLen + 1;  // строка + \n
    
    if (totalLen + insertSize >= MAX_FILE_SIZE - 2) {
        return;  // Буфер полон
    }
    
    // Глобальная позиция после текущей строки (после \n если есть)
    int insertPos = _lineOffsets[_cursor.line] + lineLen;
    if (line[lineLen] == '\n' || line[lineLen] == '\r') {
        insertPos++;  // Вставляем ПОСЛЕ \n
    }
    
    // Сдвигаем весь остаток файла вправо
    memmove(&_buffer[insertPos + insertSize],
            &_buffer[insertPos],
            totalLen - insertPos + 1);
    
    // Копируем текущую строку
    memcpy(&_buffer[insertPos], line, lineLen);
    
    // Добавляем \n
    _buffer[insertPos + lineLen] = '\n';
    
    // Перемещаем курсор на новую строку
    _cursor.line++;
    _cursor.column = 0;
    
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
    
    return result;
}

// ============================================================
// Update
// ============================================================

void Editor::update(float dt) {
    handleInput(dt);

    if (_state == EditorState::STATE_EDIT) {
        // Обычный режим - рендерим редактор
        VGA::clear(0);
        renderEditor();
        _tiles.render();
        VGA::show();
    }
    // STATE_RUNNING и STATE_FINISHED не рендерим здесь - 
    // отрисовка происходит в runLua() и callback'ах

    KEYBOARD::beginFrame();
}

void Editor::tick() {
    // Пока ничего не нужно
}