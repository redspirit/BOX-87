#pragma once

#include "ISubsystem.h"
#include "TextTiles.h"

#define MAX_PATH 32

// Класс для обработки автоповтора клавиш
class KeyRepeat {
public:
    // initialDelay - задержка перед первым повтором (сек)
    // repeatDelay - задержка между повторами (сек)
    KeyRepeat(float initialDelay = 0.5f, float repeatDelay = 0.05f);
    
    // Сброс состояния
    void reset();
    
    // Проверка клавиши с автоповтором
    // Возвращает true при первом нажатии и при автоповторе
    bool check(uint16_t key, float dt);
    
private:
    uint16_t lastKey_ = 0;
    float holdTime_ = 0.0f;
    float initialDelay_;
    float repeatDelay_;
    bool waitingForRepeat_ = false;
};

class Editor : public ISubsystem {
public:
    explicit Editor(const char* path);
    ~Editor();

    bool init() override;
    void update(float dt) override;
    void tick() override;

private:
    void parseLines();
    void handleInput(float dt);
    void ensureCursorVisible();
    void renderEditor();
    bool save();
    
    // Вставка/удаление символов
    void insertChar(uint16_t ch);
    void insertNewline();
    void deleteCharBack();
    void deleteCharForward();
    void deleteCurrentLine();
    void duplicateCurrentLine();

private:
    uint32_t _frameTimeMs = 16;
    
    // ===== Константы =====
    static constexpr size_t MAX_FILE_SIZE = 100 * 1024; // 100KB
    static constexpr size_t MAX_LINES     = 4096;

    // ===== Данные =====
    char _path[MAX_PATH];

    TextTiles _tiles;

    char      _buffer[MAX_FILE_SIZE + 1];
    uint32_t  _lineOffsets[MAX_LINES];
    uint16_t  _lineCount = 0;

    struct Cursor {
        uint16_t line   = 0;
        uint16_t column = 0;
    } _cursor;

    uint16_t _scrollX = 0;
    uint16_t _scrollY = 0;
    
    KeyRepeat keyRepeat_;
    bool _isEngLayout = true;  // по умолчанию английская раскладка
    bool _isModified = false;  // флаг несохранённых изменений
};