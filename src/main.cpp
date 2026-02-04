#include "VGA.h"
#include "AppManager.h"
#include "shell/shell.h"
#include "LOG.h"
#include <Arduino.h>
#include <AudioBeepTest.h>

const PinConfig pins(
    -1, -1, 4, 5, 6,
    -1, -1, -1, 7, 8, 9,
    -1, -1, -1, 10, 11,
    12, 13 // H, V
);

VGA vga;
Mode mode = Mode::MODE_320x240x60;
//Mode mode = Mode::MODE_640x480x60;

AppManager app;

ISubsystem* createShell() {
    return new Shell(vga, app);
}

void setup() {
	LOG.begin(115200);
	if(!vga.init(pins, mode)) while(1) delay(1);
	vga.start();

    LOG.println("Started!!!");

    app.setDefault(createShell);
    app.startDefault();

    AudioBeepTest::init();
    AudioBeepTest::play(2000);

}

void loop() {
    app.tick();
}
