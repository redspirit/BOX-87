#include "../IShellCommand.h"
#include "../shell.h"
#include "LuaRunner.h"

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
    Shell* _shell;

    LuaRunner _runner;

    static size_t sdReader(uint8_t* buffer, size_t max, void* user);

    static void cbStdout(const char* text, void* user);
    static void cbError(const char* text, void* user);

    char _path[MAX_PATH];
};