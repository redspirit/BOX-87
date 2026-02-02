#include "shell.h"
#include "palette.h"
#include "LOG.h"
#include "shell_parser.h"
#include "shell_commands.h"
#include <shell/logo.h>

#include <string.h>
#include <stdio.h>


Shell::Shell(VGA& vga, AppManager& app)
    : _vga(vga),
      _app(app),
      _console(),
      _sd(),
      _kb(), 
      _lua() {
}

Shell::~Shell() {
}

bool Shell::init() {
    paletteInit();
    _console.init(_vga, 8, 8, COLOR_WHITE);
    _console.setCursorVisible(true);
    _kb.init();

    memset(_cmd, 0, sizeof(_cmd));
    _len = 0;
    _cursorPos = 0;

    strcpy(_cwd, "/");

    _console.insertLogo(IMG_DATA, IMG_W, IMG_H, 5, 4);
    _console.printLn();
    _console.printLn();
    _console.printLn();
    _console.printLn();
    _console.printLn();
    _console.setColor(COLOR_GREEN);
    _console.printLn(" BOX-87 SYSTEM SHELL ROM v0.1");
    _console.printLn(" (c) 2026 RedSpirit");
    _console.printLn();
    _console.printLn(" SRAM 320K PSRAM 8100K");
    _console.printLn(" VGA: 320x240");
    _console.printLn(" KEYBOARD: Ready");
    _console.printLn(" SD: Unavailable");
    _console.printLn();
    _console.printLn(" Type HELP for commands");
    _console.printLn();
    _console.useDefaultColor();

    _historyCount = 0;
    _historyHead  = 0;
    _historyPos   = -1;

    printPrompt();

    return true;
}

void Shell::update(float dt) {
    _vga.clear(0);
    _console.cursorUpdate(dt);
    
    char c;
    while (_kb.getChar(c)) {
        onChar(c);
    }

    if (_kb.isJustPressed(Keyboard::BACKSPACE)) onKeyBack();
    if (_kb.isJustPressed(Keyboard::ENTER))     onKeyEnter();
    if (_kb.isJustPressed(Keyboard::LEFT))      onKeyLeft();
    if (_kb.isJustPressed(Keyboard::RIGHT))     onKeyRight();
    if (_kb.isJustPressed(Keyboard::UP))        onKeyUp();
    if (_kb.isJustPressed(Keyboard::DOWN))      onKeyDown();

    _console.show();
    _vga.show();
    _kb.beginFrame();
}

void Shell::tick() {
    _kb.poll();
}

void Shell::printPrompt() {
    _console.setColor(COLOR_CYAN);
    _console.useDefaultColor();
    _console.print(PROMPT);
    // считали актуальные координаты курсора после новой строки
    _console.getCursor(_cursorX, _cursorY);

}

void Shell::redrawInputLine() {
    // стереть старую строку
    for (int i = 0; i < SHELL_CMD_MAX; i++) {
        _console.clearCharAt(PROMPT_LEN + i, _cursorY);
    }

    // напечатать всю команду заново
    _console.setCursor(PROMPT_LEN, _cursorY);
    _console.print(_cmd);

    _console.setCursor(
        PROMPT_LEN + _cursorPos,
        _cursorY
    );
}

void Shell::setCwd(const char* path) {
    strncpy(_cwd, path, sizeof(_cwd));
    _cwd[sizeof(_cwd) - 1] = 0;
}

void Shell::onChar(char c) {
    if (_len >= SHELL_CMD_MAX - 1)
        return;

    memmove(
        &_cmd[_cursorPos + 1],
        &_cmd[_cursorPos],
        _len - _cursorPos + 1
    );

    _cmd[_cursorPos] = c;
    _len++;
    _cursorPos++;

    if (_cursorPos == _len) {
        // курсор был в конце
        _console.print(c);
    } else {
        // курсор в середине — полная перерисовка
        redrawInputLine();
    }
}

void Shell::onKeyBack() {
    if (_cursorPos == 0)
        return;

    memmove(
        &_cmd[_cursorPos - 1],
        &_cmd[_cursorPos],
        _len - _cursorPos + 1
    );

    _len--;
    _cursorPos--;

    redrawInputLine();
}

void Shell::onKeyEnter() {
    _cmd[_len] = 0;
    historyAdd(_cmd);

    _console.printLn();

    ShellParser parser;
    if (parseCommand(_cmd, parser)) {
        shellExecute(*this, parser);
    }
    

    memset(_cmd, 0, sizeof(_cmd));
    _len = 0;
    _cursorPos = 0;

    printPrompt();
}

void Shell::onKeyLeft() {
    if (_cursorPos == 0) return;
    _cursorPos--;
    redrawInputLine();
}

void Shell::onKeyRight() {
    if (_cursorPos >= _len) return;
    _cursorPos++;
    redrawInputLine();
}

void Shell::historyAdd(const char* line) {
    if (!line || !line[0]) return;

    if (_historyCount > 0) {
        int last = (_historyHead - 1 + HISTORY_SIZE) % HISTORY_SIZE;
        if (strcmp(_history[last], line) == 0)
            return;
    }

    strncpy(_history[_historyHead], line, HISTORY_CMD_MAX);
    _history[_historyHead][HISTORY_CMD_MAX - 1] = 0;

    _historyHead = (_historyHead + 1) % HISTORY_SIZE;
    if (_historyCount < HISTORY_SIZE)
        _historyCount++;

    _historyPos = -1;
}

void Shell::loadHistoryLine(const char* line) {
    strncpy(_cmd, line, SHELL_CMD_MAX);
    _cmd[SHELL_CMD_MAX - 1] = 0;

    _len = strlen(_cmd);
    _cursorPos = _len;

    redrawInputLine();
}

void Shell::onKeyUp() {
    if (_historyCount == 0) return;
    if (_historyPos < _historyCount - 1) _historyPos++;

    int idx = (_historyHead - 1 - _historyPos + HISTORY_SIZE) % HISTORY_SIZE;
    loadHistoryLine(_history[idx]);
}

void Shell::onKeyDown() {
    if (_historyPos < 0) return;

    _historyPos--;
    if (_historyPos < 0) {
        for (int i = 0; i < _len; ++i)
            _console.clearCharAt(PROMPT_LEN + i, _cursorY);

        _len = 0;
        _cursorPos = 0;
        _console.setCursor(PROMPT_LEN, _cursorY);
        return;
    }

    int idx = (_historyHead - 1 - _historyPos + HISTORY_SIZE) % HISTORY_SIZE;
    loadHistoryLine(_history[idx]);
}

void Shell::resolvePath(const char* input, char* out) {
    char temp[MAX_PATH];

    if (!input || input[0] == '\0') {
        strncpy(temp, _cwd, MAX_PATH);
    } else if (input[0] == '/') {
        strncpy(temp, input, MAX_PATH);
    } else {
        if (strcmp(_cwd, "/") == 0)
            snprintf(temp, MAX_PATH, "/%s", input);
        else
            snprintf(temp, MAX_PATH, "%s/%s", _cwd, input);
    }

    temp[MAX_PATH - 1] = 0;

    const char* segments[MAX_SEGMENTS];
    int segCount = 0;

    char* p = temp;

    // пропускаем начальный '/'
    if (*p == '/')
        p++;

    while (*p && segCount < MAX_SEGMENTS) {
        char* start = p;

        // идём до '/' или конца
        while (*p && *p != '/')
            p++;

        if (*p) {
            *p = 0;
            p++;
        }

        if (strcmp(start, ".") == 0) {
            // ничего не делаем
        }
        else if (strcmp(start, "..") == 0) {
            if (segCount > 0)
                segCount--;   // шаг назад
        }
        else if (*start) {
            segments[segCount++] = start;
        }
    }

    if (segCount == 0) {
        strcpy(out, "/");
        return;
    }

    char* dst = out;
    *dst++ = '/';

    for (int i = 0; i < segCount; ++i) {
        int l = strlen(segments[i]);
        memcpy(dst, segments[i], l);
        dst += l;

        if (i < segCount - 1)
            *dst++ = '/';
    }

    *dst = 0;
}