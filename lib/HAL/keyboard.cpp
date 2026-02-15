#include "keyboard.h"
#include <Arduino.h>
#include "soc/gpio_struct.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define PS2_CLK   38
#define PS2_DATA  39

namespace KEYBOARD {

    namespace { // Приватные данные
        constexpr int BUF_SIZE = 32;
        constexpr int EVENT_BUF_SIZE = 32;
        constexpr int BUF_MASK = BUF_SIZE - 1;
        constexpr int EVENT_BUF_MASK = EVENT_BUF_SIZE - 1;

        volatile uint8_t buffer_[BUF_SIZE];
        volatile uint8_t head_ = 0, tail_ = 0;

        KeyEvent eventBuf_[EVENT_BUF_SIZE];
        volatile uint8_t eventHead_ = 0, eventTail_ = 0;

        uint8_t keyBits_[64]{};
        uint8_t prevKeyBits_[64]{};
        KeyChar keyMap_[512]{};

        bool capsLock_ = false;
        volatile bool ledsDirty_ = false;
        volatile uint32_t ps2AckCount_ = 0;
        volatile uint32_t ps2WriteErrors_ = 0;

        volatile uint8_t bitCount_ = 0;
        volatile uint8_t incoming_ = 0;
        volatile uint32_t lastTickUs_ = 0;

        SemaphoreHandle_t mutex_ = nullptr;

        // Вспомогательные макросы переведены в инлайны
        inline int KEY_BIT(uint16_t k)  { return k & 0x1FF; }
        inline int KEY_BYTE(uint16_t k) { return KEY_BIT(k) >> 3; }
        inline int KEY_MASK(uint16_t k) { return 1 << (KEY_BIT(k) & 7); }

        inline bool readDataFast() {
            return GPIO.in1.data & (1UL << (PS2_DATA - 32));
        }
    }

// ===== ISR =====
void IRAM_ATTR handleIsr() {
    uint32_t now = micros();
    if (now - lastTickUs_ > 2000) { bitCount_ = 0; incoming_ = 0; }
    lastTickUs_ = now;

    bool val = readDataFast();
    if (bitCount_ == 0) {
        if (val) return;
        incoming_ = 0; bitCount_ = 1; return;
    }
    if (bitCount_ >= 1 && bitCount_ <= 8) {
        if (val) incoming_ |= (1 << (bitCount_ - 1));
        bitCount_++; return;
    }
    if (bitCount_ == 9) { bitCount_++; return; }
    if (bitCount_ == 10) {
        if (val) {
            uint8_t next = (head_ + 1) & BUF_MASK;
            if (next != tail_) {
                buffer_[head_] = incoming_;
                head_ = next;
            }
        }
        bitCount_ = 0;
    }
}

// ===== Внутренние утилиты записи =====
bool waitPinState(uint8_t pin, uint8_t state) {
    uint32_t start = micros();
    while (digitalRead(pin) != state) {
        if (micros() - start > 10000) return false;
    }
    return true;
}

bool ps2Write(uint8_t data) {
    detachInterrupt(PS2_CLK);
    bool parity = !(__builtin_parity(data) & 1);
    pinMode(PS2_CLK, OUTPUT);
    digitalWrite(PS2_CLK, LOW);
    delayMicroseconds(120);
    pinMode(PS2_DATA, OUTPUT);
    digitalWrite(PS2_DATA, LOW);
    delayMicroseconds(10);
    pinMode(PS2_CLK, INPUT_PULLUP);

    auto errorCleanup = []() {
        pinMode(PS2_CLK, INPUT_PULLUP);
        pinMode(PS2_DATA, INPUT_PULLUP);
        attachInterrupt(PS2_CLK, handleIsr, FALLING);
        return false;
    };

    for (uint8_t i = 0; i < 8; i++) {
        if (!waitPinState(PS2_CLK, LOW)) return errorCleanup();
        digitalWrite(PS2_DATA, (data & 1) ? HIGH : LOW);
        if (!waitPinState(PS2_CLK, HIGH)) return errorCleanup();
        data >>= 1;
    }
    if (!waitPinState(PS2_CLK, LOW)) return errorCleanup();
    digitalWrite(PS2_DATA, parity ? HIGH : LOW);
    if (!waitPinState(PS2_CLK, HIGH)) return errorCleanup();
    if (!waitPinState(PS2_CLK, LOW)) return errorCleanup();
    pinMode(PS2_DATA, INPUT_PULLUP);

    lastTickUs_ = micros();
    bitCount_ = 0;
    attachInterrupt(PS2_CLK, handleIsr, FALLING);
    waitPinState(PS2_CLK, HIGH);
    return true;
}

// ===== Публичные методы =====

void init() {
    if (!mutex_) mutex_ = xSemaphoreCreateMutex();
    
    // Инициализация KeyMap
    keyMap_[A] = {'a','A'}; keyMap_[B] = {'b','B'}; keyMap_[C] = {'c','C'};
    keyMap_[D] = {'d','D'}; keyMap_[E] = {'e','E'}; keyMap_[F] = {'f','F'};
    keyMap_[G] = {'g','G'}; keyMap_[H] = {'h','H'}; keyMap_[I] = {'i','I'};
    keyMap_[J] = {'j','J'}; keyMap_[K] = {'k','K'}; keyMap_[L] = {'l','L'};
    keyMap_[M] = {'m','M'}; keyMap_[N] = {'n','N'}; keyMap_[O] = {'o','O'};
    keyMap_[P] = {'p','P'}; keyMap_[Q] = {'q','Q'}; keyMap_[R] = {'r','R'};
    keyMap_[S] = {'s','S'}; keyMap_[T] = {'t','T'}; keyMap_[U] = {'u','U'};
    keyMap_[V] = {'v','V'}; keyMap_[W] = {'w','W'}; keyMap_[X] = {'x','X'};
    keyMap_[Y] = {'y','Y'}; keyMap_[Z] = {'z','Z'};
    keyMap_[NUM_1] = {'1','!'}; keyMap_[NUM_2] = {'2','@'}; keyMap_[NUM_3] = {'3','#'};
    keyMap_[NUM_4] = {'4','$'}; keyMap_[NUM_5] = {'5','%'}; keyMap_[NUM_6] = {'6','^'};
    keyMap_[NUM_7] = {'7','&'}; keyMap_[NUM_8] = {'8','*'}; keyMap_[NUM_9] = {'9','('};
    keyMap_[NUM_0] = {'0',')'};
    keyMap_[SPACE] = {' ',' '}; keyMap_[ENTER] = {'\n','\n'}; keyMap_[BACKSPACE] = {'\b','\b'};
    keyMap_[MINUS] = {'-','_'}; keyMap_[EQUAL] = {'=','+'}; keyMap_[LBRACKET] = {'[','{'};
    keyMap_[RBRACKET] = {']','}'}; keyMap_[BACKSLASH] = {'\\','|'}; keyMap_[SEMI] = {';',':'};
    keyMap_[QUOTE] = {'\'','"'}; keyMap_[COMMA] = {',','<'}; keyMap_[DOT] = {'.','>'};
    keyMap_[SLASH] = {'/','?'}; keyMap_[GRAVE] = {'`','~'};

    pinMode(PS2_CLK, INPUT_PULLUP);
    pinMode(PS2_DATA, INPUT_PULLUP);
    attachInterrupt(PS2_CLK, handleIsr, FALLING);
    
    delay(300);
    ps2Write(0xFF); // Reset
}

void lock() { if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY); }
void unlock() { if (mutex_) xSemaphoreGive(mutex_); }

void processScanCode(uint8_t sc) {
    static bool release = false;
    static bool extended = false;

    if (sc == 0xFA) { ps2AckCount_++; return; }
    if (sc == 0xE0) { extended = true; return; }
    if (sc == 0xF0) { release = true; return; }

    uint16_t key = sc | (extended ? 0x100 : 0);
    bool pressed = !release;

    // Обновление битовой маски
    if (pressed) keyBits_[KEY_BYTE(key)] |= KEY_MASK(key);
    else         keyBits_[KEY_BYTE(key)] &= ~KEY_MASK(key);

    // Добавление в очередь событий
    uint8_t next = (eventHead_ + 1) & EVENT_BUF_MASK;
    if (next != eventTail_) {
        eventBuf_[eventHead_] = { key, pressed, extended };
        eventHead_ = next;
    }

    if (key == CAPS && pressed) { capsLock_ = !capsLock_; ledsDirty_ = true; }
    release = false; extended = false;
}

void poll() {
    while (head_ != tail_) {
        uint8_t sc;
        noInterrupts();
        sc = buffer_[tail_];
        tail_ = (tail_ + 1) & BUF_MASK;
        interrupts();
        processScanCode(sc);
    }
    // Здесь можно добавить вызов setLeds() если нужно
}

void beginFrame() {
    lock();
    memcpy(prevKeyBits_, keyBits_, sizeof(keyBits_));
    unlock();
}

bool readKey(KeyEvent& ev) {
    if (eventHead_ == eventTail_) return false;
    lock();
    ev = eventBuf_[eventTail_];
    eventTail_ = (eventTail_ + 1) & EVENT_BUF_MASK;
    unlock();
    return true;
}

bool isPressed(uint16_t key) {
    return keyBits_[KEY_BYTE(key)] & KEY_MASK(key);
}

bool isJustPressed(uint16_t key) {
    uint8_t b = KEY_BYTE(key), m = KEY_MASK(key);
    return (keyBits_[b] & m) && !(prevKeyBits_[b] & m);
}

bool getChar(char& out) {
    KeyEvent ev;
    if (!readKey(ev) || !ev.pressed) return false;

    if (ev.key == SHIFT_LEFT || ev.key == SHIFT_RIGHT ||
        ev.key == CTRL_LEFT  || ev.key == CTRL_RIGHT) return false;

    KeyChar kc = keyMap_[ev.key];
    if (!kc.normal) return false;

    bool shift = isPressed(SHIFT_LEFT) || isPressed(SHIFT_RIGHT);
    bool upper = capsLock_ ^ shift;
    out = upper ? kc.shifted : kc.normal;
    return true;
}

uint32_t getPs2AckCount() { return ps2AckCount_; }
uint32_t getPs2WriteErrors() { return ps2WriteErrors_; }

} // namespace KEYBOARD