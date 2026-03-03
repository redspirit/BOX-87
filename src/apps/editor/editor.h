#pragma once

#include "ISubsystem.h"
#include "TextTiles.h"

#define MAX_PATH 32

class Editor : public ISubsystem {
public:
    explicit Editor(const char* path);
    ~Editor();

    bool init() override;
    void update(float dt) override;
    void tick() override;

private:
    void parseLines();
    void handleInput();
    void ensureCursorVisible();
    void renderEditor();

private:
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

};