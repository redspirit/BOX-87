#include "LuaRunner.h"
#include "sdcard.h"
#include <cstring>

// ============================================================
// Lua binding'и для console
// ============================================================

static int lua_console_print(lua_State* L) {
    ILuaConsole* console = static_cast<ILuaConsole*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (!console) return 0;

    int nargs = lua_gettop(L);

    for (int i = 1; i <= nargs; ++i) {
        const char* str = luaL_tolstring(L, i, nullptr);
        if (str) {
            console->print(str);
        }

        if (i < nargs) {
            console->print(" ");
        }
    }

    console->printLn();
    return 0;
}

static int lua_console_setcolorrgb(lua_State* L) {
    ILuaConsole* console = static_cast<ILuaConsole*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (!console) return 0;

    luaL_argcheck(L, lua_gettop(L) >= 3, 1, "setColorRGB requires 3 arguments (r, g, b)");

    int r = (int)luaL_checkinteger(L, 1);
    int g = (int)luaL_checkinteger(L, 2);
    int b = (int)luaL_checkinteger(L, 3);

    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        return luaL_error(L, "Color components must be in range 0-255");
    }
    
    // Конвертируем RGB в индекс палитры (упрощённо)
    uint8_t color = (r >> 5) | ((g >> 5) << 3) | ((b >> 6) << 6);
    console->setColorRaw(color);
    return 0;
}

static int lua_console_setcolorraw(lua_State* L) {
    ILuaConsole* console = static_cast<ILuaConsole*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (!console) return 0;
    
    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "setColorRaw requires 1 argument (color code)");
    int color = (int)luaL_checkinteger(L, 1);
    
    if (color < 0 || color > 255) {
        return luaL_error(L, "Color must be in range 0-255");
    }
    
    console->setColorRaw((uint8_t)color);
    return 0;
}

static int lua_console_setcolordefault(lua_State* L) {
    ILuaConsole* console = static_cast<ILuaConsole*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (!console) return 0;
    
    console->useDefaultColor();
    return 0;
}

static const struct luaL_Reg console_methods[] = {
    {"print",    lua_console_print},
    {"setColorRGB", lua_console_setcolorrgb},
    {"setColorRaw", lua_console_setcolorraw},
    {"setColorDefault", lua_console_setcolordefault},
    {NULL, NULL}
};

// ============================================================
// LuaRunner implementation
// ============================================================

LuaRunner::LuaRunner() : L(nullptr), _console(nullptr), _finished(false), _currentPath(nullptr) {
}

LuaRunner::~LuaRunner() {
    if (L) {
        lua_close(L);
        L = nullptr;
    }
}

bool LuaRunner::init(ILuaConsole* console) {
    _console = console;
    _finished = false;
    
    L = luaL_newstate();
    if (!L) {
        return false;
    }

    // Открываем стандартные библиотеки
    luaL_openlibs(L);
    
    // Регистрируем binding'и
    registerBindings();
    
    return true;
}

void LuaRunner::registerBindings() {
    // Создаём таблицу console
    lua_newtable(L);
    lua_pushlightuserdata(L, _console);
    luaL_setfuncs(L, console_methods, 1);
    lua_setglobal(L, "console");
}

bool LuaRunner::loadFromBuffer(const char* code, size_t len) {
    if (!L || !code) {
        return false;
    }

    // Выполняем код напрямую из буфера
    if (luaL_loadbuffer(L, code, len, "editor_buffer") != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        if (err && _console) {
            _console->print(err);
            _console->printLn();
        }
        lua_pop(L, 1);
        return false;
    }

    // Выполняем загрузку (инициализация)
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        if (err && _console) {
            _console->print(err);
            _console->printLn();
        }
        lua_pop(L, 1);
        return false;
    }

    _finished = true;
    return true;
}

const char* LuaRunner::luaSDReader(lua_State* L, void* data, size_t* size) {
    LuaRunner* self = static_cast<LuaRunner*>(data);
    
    if (!self->_currentPath) {
        *size = 0;
        return nullptr;
    }
    
    size_t read = SDCARD::read(self->_luaBuffer, sizeof(self->_luaBuffer));

    if (read == 0) {
        *size = 0;
        return nullptr; // EOF
    }

    *size = read;
    return reinterpret_cast<const char*>(self->_luaBuffer);
}

bool LuaRunner::loadFromFile(const char* path) {
    if (!L || !path) {
        return false;
    }

    if (!SDCARD::open(path)) {
        if (_console) {
            _console->print("File not found: ");
            _console->print(path);
            _console->printLn();
        }
        return false;
    }

    _currentPath = path;
    
    if (lua_load(L, luaSDReader, this, path, nullptr) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        if (err && _console) {
            _console->print(err);
            _console->printLn();
        }
        lua_pop(L, 1);
        SDCARD::close();
        return false;
    }

    SDCARD::close();
    _currentPath = nullptr;

    // Выполняем загрузку (инициализация)
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        if (err && _console) {
            _console->print(err);
            _console->printLn();
        }
        lua_pop(L, 1);
        return false;
    }

    _finished = true;
    return true;
}

void LuaRunner::setArguments(int argc, const char** argv) {
    if (!L) return;
    
    lua_newtable(L); // создаем таблицу arg

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i] ? argv[i] : "";
        lua_pushstring(L, a);
        lua_rawseti(L, -2, i - 1); // arg[0] = argv[1]
    }

    lua_setglobal(L, "arg");
}

bool LuaRunner::callMain() {
    if (!L) return false;
    
    lua_getglobal(L, "main");

    if (!lua_isfunction(L, -1)) {
        if (_console) {
            _console->print("No main() found\n");
        }
        lua_pop(L, 1);
        return false;
    }

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        if (err && _console) {
            _console->print(err);
            _console->printLn();
        }
        lua_pop(L, 1);
        return false;
    }

    return true;
}

void LuaRunner::tick() {
    // Пока ничего не нужно
    // В будущем можно добавить кооперативную многозадачность
}

void LuaRunner::cancel() {
    _finished = true;
}

bool LuaRunner::isFinished() const {
    return _finished;
}
