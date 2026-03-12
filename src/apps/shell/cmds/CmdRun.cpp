 #include "CmdRun.h"
#include "palette.h"
#include "sdcard.h"

CmdRun::CmdRun() :
    _finished(false),
    _shell(nullptr)
{
}

CmdRun::~CmdRun() {
}

size_t CmdRun::sdReader(uint8_t* buffer, size_t max, void* user) {

    CmdRun* self = (CmdRun*)user;
    (void)self;

    return SDCARD::read(buffer, max);
}

void CmdRun::cbStdout(const char* text, void* user) {

    CmdRun* self = (CmdRun*)user;
    self->_shell->console().print(text);
}

void CmdRun::cbError(const char* text, void* user) {

    CmdRun* self = (CmdRun*)user;

    auto& con = self->_shell->console();

    con.setColor(COLOR_RED);
    con.printLn(text);
    con.useDefaultColor();
}

void CmdRun::start(Shell& shell) {

    _shell = &shell;
    auto& con = shell.console();
    auto& cmd = shell.parsedCmd();

    if (cmd.argc() < 2) {

        con.setColor(COLOR_RED);
        con.printLn("Usage: RUN <file>");
        con.useDefaultColor();
        _finished = true;
        return;
    }

    shell.resolvePath(cmd.argv(1), _path);

    if (!SDCARD::open(_path)) {

        con.setColor(COLOR_RED);
        con.printLn("File not found");
        con.useDefaultColor();
        _finished = true;
        return;
    }

    if (!_runner.init()) {

        con.setColor(COLOR_RED);
        con.printLn("Lua init failed");
        con.useDefaultColor();
        _finished = true;
        return;
    }

    if (!_runner.run(sdReader,
                     this,
                     cbStdout,
                     cbError,
                     this))
    {
        _finished = true;
        SDCARD::close();
        return;
    }

    con.printLn();
    SDCARD::close();
    _finished = true;
}

void CmdRun::tick(Shell& shell) {
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