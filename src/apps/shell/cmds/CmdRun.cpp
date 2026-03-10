#include "CmdRun.h"
#include "LuaRunner.h"
#include "palette.h"
#include "sdcard.h"

// ============================================================
// Callback функции для Shell
// ============================================================

static void shell_print(void* userData, const char* text) {
    Shell* shell = static_cast<Shell*>(userData);
    if (shell) shell->console().print(text);
}

static void shell_printLn(void* userData) {
    Shell* shell = static_cast<Shell*>(userData);
    if (shell) shell->console().printLn();
}

static void shell_setColorRaw(void* userData, uint8_t color) {
    Shell* shell = static_cast<Shell*>(userData);
    if (shell) shell->console().setColorRaw(color);
}

static void shell_useDefaultColor(void* userData) {
    Shell* shell = static_cast<Shell*>(userData);
    if (shell) shell->console().useDefaultColor();
}

// ============================================================
// CmdRun implementation
// ============================================================

CmdRun::CmdRun() : _luaRunner(nullptr), _shell(nullptr), _finished(false) {
}

CmdRun::~CmdRun() {
    if (_luaRunner) {
        delete _luaRunner;
        _luaRunner = nullptr;
    }
}

void CmdRun::start(Shell& shell) {
    _shell = &shell;
    auto& con = shell.console();
    auto& cmd = shell.parsedCmd();

    const char* path = cmd.argv(1);

    if (cmd.argc() < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: RUN <file>");
        con.useDefaultColor();
        _finished = true;
        return;
    }

    // Создаём callbacks для Shell
    LuaConsoleCallbacks callbacks = {
        &shell,
        shell_print,
        shell_printLn,
        shell_setColorRaw,
        shell_useDefaultColor
    };
    
    // Создаём Lua runner
    _luaRunner = new LuaRunner();
    
    if (!_luaRunner->init(&callbacks)) {
        con.setColor(COLOR_RED);
        con.printLn("LUA state not created");
        con.useDefaultColor();
        _finished = true;
        return;
    }

    // Устанавливаем аргументы командной строки
    int argc = cmd.argc();
    const char** argv = new const char*[argc];
    for (int i = 0; i < argc; i++) {
        argv[i] = cmd.argv(i);
    }
    _luaRunner->setArguments(argc, argv);
    delete[] argv;

    // Загружаем и выполняем файл
    char pathOut[MAX_PATH];
    _shell->resolvePath(path, pathOut);
    
    if (!_luaRunner->loadFromFile(pathOut)) {
        con.setColor(COLOR_RED);
        con.printLn("LUA file not loaded");
        con.useDefaultColor();
        _finished = true;
    }
}

void CmdRun::tick(Shell& shell) {
    if (_finished || !_luaRunner)
        return;

    _luaRunner->tick();
}

void CmdRun::cancel(Shell& shell) {
    if (_luaRunner) {
        _luaRunner->cancel();
    }
    _shell->console().useDefaultColor();
    _finished = true;
}

bool CmdRun::isFinished() const {
    return _finished;
}

void CmdRun::onChar(Shell& shell, uint16_t c) {
    // Пока ничего не нужно
}
