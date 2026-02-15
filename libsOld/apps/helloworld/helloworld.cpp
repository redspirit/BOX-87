#include "helloworld.h"
#include "palette.h"

HelloWorld::HelloWorld(VGA& vga)
    : _vga(vga),
      _tiles() {
}

HelloWorld::~HelloWorld() {
}

bool HelloWorld::init() {
    paletteInit();
    _tiles.init(_vga, 8, 8);
    _tiles.print("Hello World!", 1, 1, COLOR_GREEN);
    _tiles.print("Exit after 5 sec...", 1, 3, COLOR_RED);

    _time = 0.0f;
    return true;
}

void HelloWorld::update(float dt) {
    _time += dt;

    _vga.clear(0);
    _tiles.render();
    _vga.show();

    if (_time >= 5.0f) {
        _tiles.print("EXIT", 1, 6, COLOR_RED);
        requestExit();
    }
}

void HelloWorld::tick() {

}