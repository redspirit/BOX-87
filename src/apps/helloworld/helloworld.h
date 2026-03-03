#pragma once
#include "ISubsystem.h"
#include "TextTiles.h"

class HelloWorld : public ISubsystem {
    public:
        HelloWorld();
        ~HelloWorld();

        bool init() override; // вызывается при старте
        void update(float dt) override; // вызывается с частотой 60 fps
        void tick() override;  // вызывается на столько часто на сколько позволяет процессор

    private:
        TextTiles _tiles; // доступ к тайловой карте, позволяет печатать на экране любой текст в любом месте любым цветом

        float _time;
};