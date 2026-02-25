#pragma once

class Shell;

class IShellCommand {
public:
    virtual ~IShellCommand() {}

    // virtual void start(Shell& shell, ShellParser& parser) = 0;
    virtual void start(Shell& shell) = 0;

    // вызывается каждый кадр / тик
    virtual void update(Shell& shell) = 0;

    virtual void cancel(Shell& shell) = 0;

    virtual bool isFinished() const = 0;
};