#pragma once

class ISubsystem {
    public:
        virtual ~ISubsystem() = default;

        virtual bool init() = 0;
        virtual void onExit() {}      // вызывается перед удалением

        virtual void update(float dt) = 0;
        virtual void tick() = 0;

        bool wantsExit() const { return _wantExit; }

    protected:
        void requestExit() { _wantExit = true; }

    private:
        bool _wantExit = false;

};
