#pragma once
#include "ISubsystem.h"

class AppManager {
public:
    using AppFactory = ISubsystem* (*)();

    void setDefault(AppFactory factory);
    void startDefault();

    void requestSwitch(ISubsystem* next);
    void requestExit();
    void setSubsystem(ISubsystem* s);
    void tick();

private:
    void unloadCurrent();
    void performSwitch();

    ISubsystem* _current = nullptr;
    ISubsystem* _next    = nullptr;
    AppFactory  _defaultFactory = nullptr;
    
    uint32_t _lastFrameMs = 0;
    bool _exitRequested  = false;
};