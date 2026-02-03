#include "AppManager.h"
#include <esp32-hal.h>

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

void AppManager::tick() {
    if (!_current) return;

    if (_current) {
        _current->tick();
    }

    uint32_t now = millis();
    uint32_t frameMs = _current->frameTimeMs();

    if (now - _lastFrameMs < frameMs) {
        return; // ещё рано
    }

    float dt = (now - _lastFrameMs) * 1e-3f;
    _lastFrameMs = now;

    if (dt > 0.05f) dt = 0.05f; // clamp

    _current->update(dt);

    if (_current->wantsExit()) {
        _exitRequested = true;
    }

    if (_next) {
        performSwitch();
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