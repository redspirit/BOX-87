#include "screenshot.h"
#include "BMPScreen.h"
#include "sdcard.h"
#include <stdio.h>
#include <Arduino.h>

namespace Screenshot {
    bool capture() {
        uint32_t seconds = millis() / 1000;

        char filename[32];
        snprintf(filename, sizeof(filename), "/screen_%08lu.bmp", (unsigned long)seconds);

        if (!SDCARD::open(filename, "w")) {
            return false;
        }

        File* f = SDCARD::getFile();
        BMPScreen::makeScreenShot(*f);
        SDCARD::close();

        return true;
    }
}
