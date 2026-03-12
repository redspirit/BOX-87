#include "LuaRunner.h"

LuaRunner::LuaRunner() :
    L(nullptr),
    _reader(nullptr),
    _readerUser(nullptr),
    _stdout(nullptr),
    _err(nullptr),
    _cbUser(nullptr)
{
}

LuaRunner::~LuaRunner() {
    if (L)
        lua_close(L);
}

bool LuaRunner::init() {

    L = luaL_newstate();
    if (!L)
        return false;

    luaL_openlibs(L);
    registerBindings();

    return true;
}

void LuaRunner::registerBindings() {

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, lua_print, 1);
    lua_setglobal(L, "print");

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, lua_println, 1);
    lua_setglobal(L, "printLn");
}

int LuaRunner::lua_print(lua_State* L) {

    LuaRunner* self =
        static_cast<LuaRunner*>(lua_touserdata(L, lua_upvalueindex(1)));

    if (!self || !self->_stdout)
        return 0;

    int nargs = lua_gettop(L);

    for (int i = 1; i <= nargs; ++i) {

        const char* str = luaL_tolstring(L, i, nullptr);

        if (str)
            self->_stdout(str, self->_cbUser);

        lua_pop(L, 1);

        if (i < nargs)
            self->_stdout(" ", self->_cbUser);
    }

    return 0;
}

int LuaRunner::lua_println(lua_State* L) {

    LuaRunner* self =
        static_cast<LuaRunner*>(lua_touserdata(L, lua_upvalueindex(1)));

    if (!self || !self->_stdout)
        return 0;

    int nargs = lua_gettop(L);

    for (int i = 1; i <= nargs; ++i) {

        const char* str = luaL_tolstring(L, i, nullptr);

        if (str)
            self->_stdout(str, self->_cbUser);

        lua_pop(L, 1);

        if (i < nargs)
            self->_stdout(" ", self->_cbUser);
    }

    self->_stdout("\n", self->_cbUser);

    return 0;
}

const char* LuaRunner::luaReader(lua_State* L, void* data, size_t* size) {

    LuaRunner* self = static_cast<LuaRunner*>(data);

    size_t read = self->_reader(self->_buffer,
                                LUA_READ_BUFFER,
                                self->_readerUser);

    if (read == 0) {
        *size = 0;
        return nullptr;
    }

    *size = read;
    return (const char*)self->_buffer;
}

bool LuaRunner::run(ReadCallback reader,
                    void* readerUserData,
                    StdoutCallback stdoutCb,
                    ErrorCallback errCb,
                    void* callbackUserData)
{
    _reader = reader;
    _readerUser = readerUserData;

    _stdout = stdoutCb;
    _err = errCb;

    _cbUser = callbackUserData;

    if (lua_load(L, luaReader, this, "chunk", nullptr) != LUA_OK) {

        const char* err = lua_tostring(L, -1);

        if (err && _err)
            _err(err, _cbUser);

        lua_pop(L, 1);
        return false;
    }

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {

        const char* err = lua_tostring(L, -1);

        if (err && _err)
            _err(err, _cbUser);

        lua_pop(L, 1);
        return false;
    }

    return true;
}