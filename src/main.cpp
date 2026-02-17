#include <Arduino.h>
#include "VGA/VGA.h"
#include "keyboard.h"
#include "sdcard.h"
#include "AppManager.h"
#include "apps/shell/shell.h"
#include "LOG.h"


//Mode vgaMode = Mode::MODE_320x240x60;
Mode vgaMode = Mode::MODE_640x480x60;

AppManager app;
ISubsystem* createShell() {
    return new Shell(app);
}

void setup() {
	LOG.begin(115200);

    if(!VGA::init(vgaMode, 8)) while(1) delay(1);
    VGA::start();
    LOG.println("VGA started");

    KEYBOARD::init();
    LOG.println("Keyboard init");

    SDCARD::init();

    app.setDefault(createShell);
    app.startDefault();
}

void loop() {
    app.tick();
    KEYBOARD::poll();
}
