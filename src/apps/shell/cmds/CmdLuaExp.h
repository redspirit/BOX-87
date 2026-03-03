#include "../IShellCommand.h"
#include "../shell.h"
#include "palette.h"
#include <string.h>
#include <stdio.h>

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
    #include "lualib.h"
}

class CmdLuaExp : public IShellCommand {
private:
    bool _finished = false;
    bool runExpression(const char* expr, char* out, size_t outSize) {
    if (!expr || !out || outSize == 0)
        return false;

    out[0] = 0;

    lua_State* L = luaL_newstate();
    if (!L) {
        strncpy(out, "Cannot create Lua state", outSize - 1);
        out[outSize - 1] = 0;
        return false;
    }

    luaL_openlibs(L);

    // form: return <expr>
    char buf[256];
    snprintf(buf, sizeof(buf), "return %s", expr);

    if (luaL_dostring(L, buf) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        if (err) {
            strncpy(out, err, outSize - 1);
            out[outSize - 1] = 0;
        }
        lua_close(L);
        return false;
    }

    if (lua_gettop(L) == 0) {
        strncpy(out, "nil", outSize - 1);
        out[outSize - 1] = 0;
        lua_close(L);
        return true;
    }

    const char* str = luaL_tolstring(L, -1, nullptr);
    if (!str) {
        strncpy(out, "<non-printable>", outSize - 1);
        out[outSize - 1] = 0;
        lua_close(L);
        return true;
    }

    strncpy(out, str, outSize - 1);
    out[outSize - 1] = 0;

    lua_close(L);
    _finished = true;
    return true;
}

public:

    void start(Shell& shell) override {
        auto& con = shell.console();
        auto& cmd = shell.parsedCmd();        
        _finished = false;

        const char* expr = cmd.argv(1);

        if (cmd.argc() < 2) {
            con.setColor(COLOR_RED);
            con.printLn("Usage: LUA <expression>");
            con.useDefaultColor();
            return;
        }

        char out[128];

        if (!runExpression(expr, out, sizeof(out))) {
            con.setColor(COLOR_RED);
            con.printLn(out[0] ? out : "Lua error");
            con.useDefaultColor();
            return;
        }

        con.printLn(out);

    }

    void tick(Shell& shell) override {

    }

    void cancel(Shell& shell) override {
        _finished = true;
    }

    bool isFinished() const override {
        return _finished;
    }

    void onChar(Shell& shell, uint16_t c) {
        
    }
};