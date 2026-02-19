#include "helloworld.h"
#include "palette.h"
#include "VGA/VGA.h"

HelloWorld::HelloWorld()
    : _tiles() {
}

HelloWorld::~HelloWorld() {
}

bool HelloWorld::init() {
    paletteInit();
    _tiles.init();
    _tiles.print("ZERO", 0, 0, 255);
    _tiles.print("Hello World!", 1, 1, COLOR_GREEN);
    _tiles.print("Exit after 5 sec...", 1, 3, COLOR_RED);

    _time = 0.0f;
    return true;
}

void HelloWorld::update(float dt) {
    _time += dt;

    VGA::clear(0);
    _tiles.render();
    VGA::show();

    if (_time >= 5.0f) {
        _tiles.print("EXIT", 1, 6, COLOR_RED);
        requestExit();
    }
}

void HelloWorld::tick() {

}