#pragma once
#include "ISubsystem.h"
#include "TextTiles.h"

class HelloWorld : public ISubsystem {
    public:
        HelloWorld();
        ~HelloWorld();

        bool init() override;
        void update(float dt) override;
        void tick() override;

    private:
        TextTiles _tiles;

        float _time;
};