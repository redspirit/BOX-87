#pragma once

class Shell;

class IShellCommand {
public:
    virtual ~IShellCommand() {}

    virtual void start(Shell& shell) = 0;

    // вызывается каждый тик
    virtual void tick(Shell& shell) = 0;

    virtual void cancel(Shell& shell) = 0;

    virtual bool isFinished() const = 0;

    virtual void onChar(Shell& shell, uint16_t c) {}
};