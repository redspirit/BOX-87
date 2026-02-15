#pragma once
#include "ISubsystem.h"
#include "TextTiles.h"
#include "VGA.h"

class HelloWorld : public ISubsystem {
    public:
        HelloWorld(VGA& _vga);
        ~HelloWorld();

        bool init() override;
        void update(float dt) override;
        void tick() override;

    private:
        VGA& _vga;
        TextTiles _tiles;

        float _time;
};