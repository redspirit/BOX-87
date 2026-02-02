#include "AppManager.h"

void AppManager::setDefault(AppFactory factory) {
    _defaultFactory = factory;
}

void AppManager::startDefault() {
    if (_defaultFactory) {
        setSubsystem(_defaultFactory());
    }
}

void AppManager::requestSwitch(ISubsystem* next) {
    _next = next;
}

void AppManager::setSubsystem(ISubsystem* s) {
    unloadCurrent();

    _current = s;
    if (_current) {
        if (!_current->init()) {
            unloadCurrent();
            startDefault();
        }
    }
}

void AppManager::requestExit() {
    _exitRequested = true;
}

void AppManager::unloadCurrent() {
    if (_current) {
        _current->onExit();
        delete _current;
        _current = nullptr;
    }
}

void AppManager::update(float dt) {
    if (_current) {
        _current->update(dt);

        if (_current->wantsExit()) {
            _exitRequested = true;
        }
    }

    if (_exitRequested || _next) {
        performSwitch();
    }
}

void AppManager::tick() {
    if (_current) {
        _current->tick();
    }
}

void AppManager::performSwitch() {
    if (_current) {
        _current->onExit();
        delete _current;
        _current = nullptr;
    }

    _exitRequested = false;

    if (_next) {
        _current = _next;
        _next = nullptr;
        _current->init();
    } else {
        startDefault();
    }
}