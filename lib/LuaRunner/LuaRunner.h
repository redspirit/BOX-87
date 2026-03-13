#pragma once

#include <stddef.h>
#include <stdint.h>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

class LuaRunner {
public:

    typedef size_t (*ReadCallback)(uint8_t* buffer, size_t maxSize, void* userData);
    typedef void (*StdoutCallback)(const char* text, void* userData);
    typedef void (*ErrorCallback)(const char* text, void* userData);

    LuaRunner();
    ~LuaRunner();

    bool init();

    bool run(ReadCallback reader,
             void* readerUserData,
             StdoutCallback stdoutCb,
             ErrorCallback errCb,
             void* callbackUserData);

private:

    lua_State* L;

    static const size_t LUA_READ_BUFFER = 512;
    uint8_t _buffer[LUA_READ_BUFFER];

    ReadCallback _reader;
    void* _readerUser;

    StdoutCallback _stdout;
    ErrorCallback _err;

    void* _cbUser;

    static const char* luaReader(lua_State* L, void* data, size_t* size);

    static int lua_print(lua_State* L);
    static int lua_println(lua_State* L);

    void registerBindings();
};