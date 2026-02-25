#include "IShellCommand.h"
#include "shell.h"

class CmdPing : public IShellCommand {
private:
    uint32_t _lastTick = 0;
    uint32_t _counter = 0;
    bool _finished = false;

public:

    void start(Shell& shell) override {
        _lastTick = millis();
        _counter = 0;
        _finished = false;

        shell.console().printLn("Starting ping (Ctrl+C to stop)");
    }

    void tick(Shell& shell) override {

        if (_finished)
            return;

        uint32_t now = millis();

        if (now - _lastTick >= 1000) {
            _lastTick = now;

            _counter++;

            auto& con = shell.console();
            con.print("Ping ok - ");
            con.printInt(_counter);
            con.printLn();
        }
    }

    void cancel(Shell& shell) override {

        shell.console().printLn("^C");
        shell.console().printLn("Ping stopped");

        _finished = true;
    }

    bool isFinished() const override {
        return _finished;
    }

    void onChar(Shell& shell, uint16_t c) {
        
    }
};