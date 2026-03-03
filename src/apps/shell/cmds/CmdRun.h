#include "../IShellCommand.h"
#include "../shell.h"

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
    #include "lualib.h"
}

class CmdRun : public IShellCommand {
    public:
        CmdRun();
        ~CmdRun();    
        void start(Shell& shell) override;
        void tick(Shell& shell) override;
        void cancel(Shell& shell) override;
        bool isFinished() const override;
        void onChar(Shell& shell, uint16_t c);

    private:
        bool _finished = false;
        lua_State* L;
        Shell* _shell;

        static const size_t LUA_READ_BUFFER = 512;
        uint8_t _luaBuffer[LUA_READ_BUFFER];
        static const char* luaSDReader(lua_State* L, void* data, size_t* size);
        
        bool runFile(const char* path);
        void registerBindings();
        bool callMain();
};