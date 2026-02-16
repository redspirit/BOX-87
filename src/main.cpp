#include <Arduino.h>
#include "VGA/VGA.h"
#include "keyboard.h"
#include "sdcard.h"
// #include "AppManager.h"
// #include "shell/shell.h"
#include "LOG.h"

const PinConfig vgaPins(
    4, 5, 6,
    7, 9, 8,
    11, 10, 14,
    12, 13 // H, V
);

// Mode vgaMode = Mode::MODE_320x240x60;
Mode vgaMode = Mode::MODE_640x480x60;

// AppManager app;
// ISubsystem* createShell() {
//     return new Shell(vga, app);
// }

void setup() {
	LOG.begin(115200);

    if(!VGA::init(vgaPins, vgaMode, 8)) while(1) delay(1);
    VGA::start();
    LOG.println("VGA started");

    KEYBOARD::init();
    LOG.println("Keyboard init");

    SDCARD::init();

    VGA::fillRect8(20, 30, 100, 100, 128);
    VGA::show();

    // app.setDefault(createShell);
    // app.startDefault();
}

void loop() {
    // app.tick();
    KEYBOARD::poll();
}
