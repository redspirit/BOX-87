#include "CmdRun.h"
#include "palette.h"
#include "sdcard.h"


static int lua_console_print(lua_State* L) {
    Shell* shell = static_cast<Shell*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (!shell) return 0;

    auto& console = shell->console();
    int nargs = lua_gettop(L);

    for (int i = 1; i <= nargs; ++i) {
        const char* str = luaL_tolstring(L, i, nullptr);
        if (str) {
            console.print(str);
        }

        if (i < nargs) {
            console.print(" ");
        }
    }

    console.printLn();
    return 0;
}

static int lua_console_setcolorrgb(lua_State* L) {
    Shell* shell = static_cast<Shell*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (!shell) return 0;

    // 2. Проверяем, что передано ровно 3 аргумента (или используй lua_gettop(L) < 3 для проверки минимума)
    // luaL_argcheck выбрасывает ошибку, если условие ложно
    luaL_argcheck(L, lua_gettop(L) >= 3, 1, "setColorRGB requires 3 arguments (r, g, b)");

    // 3. Проверяем типы аргументов
    // luaL_checkinteger автоматически выдаст ошибку, если аргумент нельзя привести к числу
    int r = (int)luaL_checkinteger(L, 1);
    int g = (int)luaL_checkinteger(L, 2);
    int b = (int)luaL_checkinteger(L, 3);

    // Дополнительная валидация (если r,g,b должны быть в диапазоне 0-255)
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        return luaL_error(L, "Color components must be in range 0-255");
    }
    shell->console().setColorRaw(rgb332(r, g, b));
    return 0;
}

static int lua_console_setcolorraw(lua_State* L) {
    Shell* shell = static_cast<Shell*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (!shell) return 0;
    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "setColorRaw requires 1 argument (color code)");
    int color = (int)luaL_checkinteger(L, 1);
    if (color < 0 || color > 255) {
        return luaL_error(L, "Color must be in range 0-255");
    }
    shell->console().setColorRaw((uint8_t)color);
    return 0;
}

static int lua_console_setcolordefault(lua_State* L) {
    Shell* shell = static_cast<Shell*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (!shell) return 0;
    shell->console().useDefaultColor();
    return 0;
}


static const struct luaL_Reg console_methods[] = {
    {"print",    lua_console_print},
    {"setColorRGB", lua_console_setcolorrgb},
    {"setColorRaw", lua_console_setcolorraw},
    {"setColorDefault", lua_console_setcolordefault},
    {NULL, NULL} // Маркер конца массива
};


CmdRun::CmdRun() : L(nullptr), _shell(nullptr) {
     _finished = false;
}

CmdRun::~CmdRun() {
    if (L)
        lua_close(L);
}

void CmdRun::registerBindings() {
    // 1. Создаем новую таблицу (наш будущий объект "console")
    lua_newtable(L);
    lua_pushlightuserdata(L, _shell);
    luaL_setfuncs(L, console_methods, 1);
    lua_setglobal(L, "console");
}

bool CmdRun::callMain() {
    lua_getglobal(L, "main");

    if (!lua_isfunction(L, -1)) {
        _shell->console().print("No main() found\n");
        lua_pop(L, 1);
        return false;
    }

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        if (err)
            _shell->console().print(err);

        lua_pop(L, 1);
        return false;
    }

    return true;
}

const char* CmdRun::luaSDReader(lua_State* L, void* data, size_t* size) {
    CmdRun* self = static_cast<CmdRun*>(data);
    size_t read = SDCARD::read(self->_luaBuffer, LUA_READ_BUFFER);

    if (read == 0) {
        *size = 0;
        return nullptr; // EOF
    }

    *size = read;
    return reinterpret_cast<const char*>(self->_luaBuffer);
}

bool CmdRun::runFile(const char* path) {
    if (!L)
        return false;

    char pathOut[MAX_PATH];
    _shell->resolvePath(path, pathOut);

    if(!SDCARD::open(pathOut)) {
        _shell->console().printLn("File not found");
        SDCARD::close();
        return false;
    }

    if (lua_load(L,
                 luaSDReader,
                 this,        // передаём объект
                 pathOut,
                 nullptr) != LUA_OK)
    {
        const char* err = lua_tostring(L, -1);
        if (err)
            _shell->console().print(err);

        lua_pop(L, 1);
        SDCARD::close();
        return false;
    }

    SDCARD::close();

    // Выполняем файл (инициализация)
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        if (err)
            _shell->console().print(err);

        lua_pop(L, 1);
        return false;
    }

    _finished = true;
    _shell->console().useDefaultColor();
    return true;
    // return callMain();
};

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

    L = luaL_newstate();
    if (!L) {
        con.setColor(COLOR_RED);
        con.printLn("LUA state not created");
        con.useDefaultColor();
        _finished = true;
        return;
    }

    luaL_openlibs(L);

    registerBindings();

    if(!runFile(path)) {
        con.setColor(COLOR_RED);
        con.printLn("LUA file not loaded");
        con.useDefaultColor();
        _finished = true;
    }

}

void CmdRun::tick(Shell& shell) {
    if (_finished)
        return;

}

void CmdRun::cancel(Shell& shell) {
    _shell->console().useDefaultColor();
    _finished = true;
}

bool CmdRun::isFinished() const {
    return _finished;
}

void CmdRun::onChar(Shell& shell, uint16_t c) {
    
}
