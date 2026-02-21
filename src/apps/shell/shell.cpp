#include "shell.h"
#include "palette.h"
#include "LOG.h"
#include "shell_parser.h"
#include "shell_commands.h"
#include <apps/shell/logo.h>
#include <string.h>
#include <stdio.h>
#include "BMPScreen.h"
#include "sdcard.h"

Shell::Shell(AppManager& app)
    : _app(app),
      _console(),
      _lua() {
}

Shell::~Shell() {
}

bool Shell::init() {
    paletteInit();
    _console.init(255); // default color white
    _console.setCursorVisible(true);

    memset(_cmd, 0, sizeof(_cmd));
    _len = 0;
    _cursorPos = 0;

    strcpy(_cwd, "/");

    _console.insertLogo(IMG_DATA, IMG_W, IMG_H, 0, 1);
    _console.printLn();
    _console.printLn();
    _console.printLn();
    _console.printLn();
    _console.setColor(COLOR_GREEN);
    _console.printLn(" BOX-87 SYSTEM SHELL ROM v0.1");
    _console.printLn(" (c) 2026 RedSpirit");
    _console.printLn();
    _console.printLn(" SRAM 388K PSRAM 8191K");
    _console.print(" VGA: "); 
    _console.printInt(VGA::width()); _console.print("x"); 
    _console.printInt(VGA::height()); _console.printLn();
    _console.printLn(" KEYBOARD: Ready");
    _console.print(" SD: ");
    if (SDCARD::init()) {
        uint64_t total = SDCARD::totalBytes();
        uint64_t free  = SDCARD::freeBytes();
        _console.printInt(int(free / (1024 * 1024)));
        _console.print("/");
        _console.printInt(int(total / (1024 * 1024)));
        _console.print(" MB (");
        _console.printInt(int((free * 100ULL + total / 2) / total));
        _console.printLn("%)");
        SDCARD::close();
    } else {
        _console.printLn("Unavailable");
    }
    _console.printLn();
    _console.print(" ");
    _console.setColor(2); _console.print("█");
    _console.setColor(3); _console.print("█");
    _console.setColor(4); _console.print("█");
    _console.setColor(5); _console.print("█");
    _console.setColor(6); _console.print("█");
    _console.setColor(7); _console.print("█");
    _console.setColor(8); _console.print("█");
    _console.setColor(9); _console.print("█");
    _console.printLn();
    _console.printLn();
    _console.setColor(COLOR_GREEN);
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
    VGA::clear(0);
    _console.cursorUpdate(dt);
    
    char c;
    while (KEYBOARD::getChar(c)) {
        onChar(c);
    }

    if (KEYBOARD::isJustPressed(KEYBOARD::BACKSPACE)) onKeyBack();
    if (KEYBOARD::isJustPressed(KEYBOARD::ENTER))     onKeyEnter();
    if (KEYBOARD::isJustPressed(KEYBOARD::LEFT))      onKeyLeft();
    if (KEYBOARD::isJustPressed(KEYBOARD::RIGHT))     onKeyRight();
    if (KEYBOARD::isJustPressed(KEYBOARD::UP))        onKeyUp();
    if (KEYBOARD::isJustPressed(KEYBOARD::DOWN))      onKeyDown();

    _console.show();
    if (KEYBOARD::isJustPressed(KEYBOARD::PRINT_SCREEN)) onPrintScreen(); 

    VGA::show();
    KEYBOARD::beginFrame();
}

void Shell::tick() {

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
        _console.printRawChar(c);
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

void Shell::onPrintScreen() {
    uint32_t seconds = millis() / 1000;

    char filename[32];
    snprintf(filename, sizeof(filename), "/screen_%08lu.bmp", (unsigned long)seconds);

    SDCARD::open(filename, "w");

    File* f = SDCARD::getFile();
    if (!f) {
        LOG.println("NO FILE");
        _console.print("File not open"); 
        return;
    }

    BMPScreen::makeScreenShot(*f);
    SDCARD::close();
    _console.print("Screenshot: ");
    _console.printLn(filename);
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