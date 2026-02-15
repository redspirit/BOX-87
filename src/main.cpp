#include <Arduino.h>
#include "vga.h"
#include "palette.h"

void setup() {
    Serial.begin(115200);
    Serial.println("Starting VGA via esp_lcd...");

    VGA::init();

    // VGA::clear(rgb332(128, 0, 0));
    // VGA::show();
}

void loop() {
    VGA::clear(rgb332(128, 0, 0));

    static int x = 0;
    static int y = 100;
    static int dirX = 2;
    static int dirY = 2;
    
    VGA::fillRect(x, y, 50, 50, rgb332(255, 0, 0)); 
    
    x += dirX;
    y += dirY;
    if (x > VGA::width() - 50 || x < 0) dirX = -dirX;
    if (y > VGA::height() - 50 || y < 0) dirY = -dirY;

    VGA::show();
}