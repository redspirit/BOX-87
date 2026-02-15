#pragma once
#include <stdint.h>
#include <stddef.h>

namespace KEYBOARD {

    struct KeyEvent {
        uint16_t key;
        bool pressed;
        bool extended;
    };

    struct KeyChar {
        char normal;
        char shifted;
    };

    // Скан-коды (те же, что в вашем классе)
    enum Key : uint16_t {
        ESC = 0x76,
        F1 = 0x05, F2 = 0x06, F3 = 0x04, F4 = 0x0C,
        F5 = 0x03, F6 = 0x0B, F7 = 0x83, F8 = 0x0A,
        F9 = 0x01, F10 = 0x09, F11 = 0x78, F12 = 0x07,
        NUM_1 = 0x16, NUM_2 = 0x1E, NUM_3 = 0x26, NUM_4 = 0x25,
        NUM_5 = 0x2E, NUM_6 = 0x36, NUM_7 = 0x3D, NUM_8 = 0x3E,
        NUM_9 = 0x46, NUM_0 = 0x45,
        BACKSLASH = 0x5D, BACKSPACE = 0x66, TAB = 0x0D, CAPS = 0x58,
        SHIFT_LEFT = 0x12, SHIFT_RIGHT = 0x59, CTRL_LEFT = 0x14, CTRL_RIGHT = 0x114,
        ALT_LEFT = 0x11, ALT_RIGHT = 0x111, SPACE = 0x29, ENTER = 0x5A,
        MINUS = 0x4E, EQUAL = 0x55, GRAVE = 0x0E, LBRACKET = 0x54, RBRACKET = 0x5B,
        SEMI = 0x4C, QUOTE = 0x52, COMMA = 0x41, DOT = 0x49, SLASH = 0x4A,
        Q = 0x15, W = 0x1D, E = 0x24, R = 0x2D, T = 0x2C,
        Y = 0x35, U = 0x3C, I = 0x43, O = 0x44, P = 0x4D,
        A = 0x1C, S = 0x1B, D = 0x23, F = 0x2B, G = 0x34,
        H = 0x33, J = 0x3B, K = 0x42, L = 0x4B,
        Z = 0x1A, X = 0x22, C = 0x21, V = 0x2A,
        B = 0x32, N = 0x31, M = 0x3A,
        RIGHT = 0x174, LEFT = 0x16B, UP = 0x175, DOWN = 0x172,
    };

    void init();
    void poll();
    void beginFrame();

    bool readKey(KeyEvent& ev);
    bool getChar(char& out);

    bool isPressed(uint16_t key);
    bool isJustPressed(uint16_t key);
    bool isJustReleased(uint16_t key);

    uint32_t getPs2AckCount();
    uint32_t getPs2WriteErrors();
    
    // Блокировка для многопоточности
    void lock();
    void unlock();
}