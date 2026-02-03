#pragma once
#include <stdint.h>

class ISubsystem {
    public:
        virtual ~ISubsystem() = default;

        virtual bool init() = 0;
        virtual void onExit() {}      // вызывается перед удалением

        virtual void update(float dt) = 0;
        virtual void tick() = 0;

        bool wantsExit() const { return _wantExit; }
        virtual uint32_t frameTimeMs() const {
            return 16; // default ~60 FPS
        }

    protected:
        void requestExit() { _wantExit = true; }

    private:
        bool _wantExit = false;

};
