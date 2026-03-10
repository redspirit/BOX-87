#include "../IShellCommand.h"
#include "../shell.h"

class LuaRunner;

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
        bool _finished;
        LuaRunner* _luaRunner;
        Shell* _shell;
};