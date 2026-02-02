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
    void update(float dt);
    void tick();

private:
    void unloadCurrent();
    void performSwitch();

    ISubsystem* _current = nullptr;
    ISubsystem* _next    = nullptr;
    AppFactory  _defaultFactory = nullptr;
    
    bool _exitRequested  = false;
};