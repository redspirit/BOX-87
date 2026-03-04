#include "helloworld.h"
#include "keyboard.h"
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
    _tiles.foregroundVisible(true);

    _tiles.print("Hello World!", 1, 1, getColorByPalette(COLOR_GREEN), getColorByPalette(COLOR_BLUE), false, false);
    _tiles.print("Exit after 20 sec...", 1, 3, getColorByPalette(COLOR_RED), 0, true, false);
    _tiles.print("Or press ESC to exit", 1, 4, getColorByPalette(COLOR_YELLOW));

    _tiles.drawTile(10, 10, {'X', getColorByPalette(COLOR_WHITE), 0, false, true});
    _tiles.drawTileForeground(10, 10, {' ', getColorByPalette(COLOR_YELLOW), true, true});

    _time = 0.0f;
    return true;
}

void HelloWorld::update(float dt) {
    _time += dt;

    if (_time >= 20.0f) {
        requestExit();
    }

    if (KEYBOARD::isJustPressed(KEYBOARD::ESC)) {
        requestExit();
    }

    VGA::clear(0);
    _tiles.render();
    VGA::show();
    KEYBOARD::beginFrame();
}

void HelloWorld::tick() {

}