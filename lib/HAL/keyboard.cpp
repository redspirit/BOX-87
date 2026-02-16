#include "keyboard.h"
#include "LOG.h"
#include <Arduino.h>

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

    void IRAM_ATTR isrTrampoline() {
        handleIsr();
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

        for (uint8_t i = 0; i < 8; i++) {
            if (!waitPinState(PS2_CLK, LOW)) goto error;
            digitalWrite(PS2_DATA, (data & 1) ? HIGH : LOW);
            if (!waitPinState(PS2_CLK, HIGH)) goto error;
            data >>= 1;
        }

        if (!waitPinState(PS2_CLK, LOW)) goto error;
        digitalWrite(PS2_DATA, parity ? HIGH : LOW);
        if (!waitPinState(PS2_CLK, HIGH)) goto error;

        if (!waitPinState(PS2_CLK, LOW)) goto error;
        pinMode(PS2_DATA, INPUT_PULLUP);

        lastTickUs_ = micros();
        bitCount_ = 0;
        attachInterrupt(PS2_CLK, isrTrampoline, FALLING);

        waitPinState(PS2_CLK, HIGH);
        return true;

    error:
        pinMode(PS2_CLK, INPUT_PULLUP);
        pinMode(PS2_DATA, INPUT_PULLUP);
        attachInterrupt(PS2_CLK, isrTrampoline, FALLING);
        return false;
    }

    // ===== Публичные методы =====

    bool available() {
        return head_ != tail_;
    }

    uint8_t readRaw() {
        noInterrupts();
        uint8_t v = buffer_[tail_];
        tail_ = (tail_ + 1) & BUF_MASK;
        interrupts();
        return v;
    }

    void initKeyMap() {
        keyMap_[A] = {'a','A'};
        keyMap_[B] = {'b','B'};
        keyMap_[C] = {'c','C'};
        keyMap_[D] = {'d','D'};
        keyMap_[E] = {'e','E'};
        keyMap_[F] = {'f','F'};
        keyMap_[G] = {'g','G'};
        keyMap_[H] = {'h','H'};
        keyMap_[I] = {'i','I'};
        keyMap_[J] = {'j','J'};
        keyMap_[K] = {'k','K'};
        keyMap_[L] = {'l','L'};
        keyMap_[M] = {'m','M'};
        keyMap_[N] = {'n','N'};
        keyMap_[O] = {'o','O'};
        keyMap_[P] = {'p','P'};
        keyMap_[Q] = {'q','Q'};
        keyMap_[R] = {'r','R'};
        keyMap_[S] = {'s','S'};
        keyMap_[T] = {'t','T'};
        keyMap_[U] = {'u','U'};
        keyMap_[V] = {'v','V'};
        keyMap_[W] = {'w','W'};
        keyMap_[X] = {'x','X'};
        keyMap_[Y] = {'y','Y'};
        keyMap_[Z] = {'z','Z'};

        keyMap_[NUM_1] = {'1','!'};
        keyMap_[NUM_2] = {'2','@'};
        keyMap_[NUM_3] = {'3','#'};
        keyMap_[NUM_4] = {'4','$'};
        keyMap_[NUM_5] = {'5','%'};
        keyMap_[NUM_6] = {'6','^'};
        keyMap_[NUM_7] = {'7','&'};
        keyMap_[NUM_8] = {'8','*'};
        keyMap_[NUM_9] = {'9','('};
        keyMap_[NUM_0] = {'0',')'};

        keyMap_[SPACE] = {' ',' '};
        keyMap_[MINUS] = {'-','_'};
        keyMap_[EQUAL] = {'=','+'};
        keyMap_[LBRACKET] = {'[','{'};
        keyMap_[RBRACKET] = {']','}'};
        keyMap_[BACKSLASH] = {'\\','|'};
        keyMap_[SEMI] = {';',':'};
        keyMap_[QUOTE] = {'\'','"'};
        keyMap_[COMMA] = {',','<'};
        keyMap_[DOT] = {'.','>'};
        keyMap_[SLASH] = {'/','?'};
        keyMap_[GRAVE] = {'`','~'};
    }

    void init() {
        initKeyMap();

        pinMode(PS2_CLK, INPUT_PULLUP);
        pinMode(PS2_DATA, INPUT_PULLUP);
        attachInterrupt(PS2_CLK, isrTrampoline, FALLING);

        delay(300);

        // RESET
        ps2Write(0xFF);

        bool ack = false;
        bool bat = false;
        uint32_t t = millis();

        while (millis() - t < 300) {
            if (available()) {
                uint8_t b = readRaw();
                if (b == 0xFA) ack = true;
                if (b == 0xAA) bat = true;
            }
        }

        if (ack && bat) {
            ps2Write(0xED);
            t = millis();
            while (millis() - t < 100) {
                if (available() && readRaw() == 0xFA)
                    break;
            }
            ps2Write(0x03);
        }
    }

    // ===== Scan code processing =====
    void pushEvent(uint16_t key, bool pressed, bool extended) {
        if (pressed)
            keyBits_[KEY_BYTE(key)] |=  KEY_MASK(key);
        else
            keyBits_[KEY_BYTE(key)] &= ~KEY_MASK(key);

        uint8_t next = (eventHead_ + 1) & EVENT_BUF_MASK;
        if (next == eventTail_)
            return;

        eventBuf_[eventHead_] = { key, pressed, extended };
        eventHead_ = next;
    }

    void processScanCode(uint8_t sc) {
        static bool release = false;
        static bool extended = false;

        if (sc == 0xFA) {
            ps2AckCount_++;
            return;
        }

        if (sc == 0xE0) {
            extended = true;
            return;
        }

        if (sc == 0xF0) {
            release = true;
            return;
        }

        uint16_t key = sc | (extended ? 0x100 : 0);
        bool pressed = !release;

        pushEvent(key, pressed, extended);

        if (key == CAPS && pressed) {
            capsLock_ = !capsLock_;
            ledsDirty_ = true;
        }

        release = false;
        extended = false;
    }

    bool waitForAck(uint32_t timeoutMs) {
        uint32_t start = millis();
        uint32_t initialAck = ps2AckCount_; 
        
        while (millis() - start < timeoutMs) {
            // Если в буфере что-то появилось - обрабатываем
            while (available()) {
                processScanCode(readRaw());
            }
            // Если счетчик ACK увеличился - значит мы получили 0xFA
            if (ps2AckCount_ > initialAck) return true;
            
            yield(); // Даем заняться фоновыми задачами
        }
        return false;
    }

    void setLeds() {
        uint8_t leds = 0;
        if (capsLock_)
            leds |= (1 << 2);

        if (ps2Write(0xED)) {
            if (!waitForAck(200))
                ps2WriteErrors_++;
        } else {
            ps2WriteErrors_++;
            return;
        }

        if (ps2Write(leds)) {
            if (!waitForAck(200))
                ps2WriteErrors_++;
        } else {
            ps2WriteErrors_++;
        }
    }

    void poll() {
        while (available())
            processScanCode(readRaw());

        if (ledsDirty_) {
            setLeds();
            ledsDirty_ = false;
        }
    }

    void beginFrame() {
        memcpy(prevKeyBits_, keyBits_, sizeof(keyBits_));
    }

    bool readKey(KeyEvent& ev) {
        if (eventHead_ == eventTail_)
            return false;

        noInterrupts();
        ev = eventBuf_[eventTail_];
        eventTail_ = (eventTail_ + 1) & EVENT_BUF_MASK;
        interrupts();
        return true;
    }

    bool isPressed(uint16_t key) {
        return keyBits_[KEY_BYTE(key)] & KEY_MASK(key);
    }

    bool isJustPressed(uint16_t key) {
        uint8_t b = KEY_BYTE(key), m = KEY_MASK(key);
        return (keyBits_[b] & m) && !(prevKeyBits_[b] & m);
    }

    bool isJustReleased(uint16_t key) {
        uint8_t b = KEY_BYTE(key), m = KEY_MASK(key);
        return !(keyBits_[b] & m) && (prevKeyBits_[b] & m);
    }

    bool getChar(char& out) {
        KeyEvent ev;
        if (!readKey(ev) || !ev.pressed)
            return false;

        if (ev.key == SHIFT_LEFT || ev.key == SHIFT_RIGHT ||
            ev.key == CTRL_LEFT  || ev.key == CTRL_RIGHT  ||
            ev.key == ALT_LEFT   || ev.key == ALT_RIGHT)
            return false;

        KeyChar kc = keyMap_[ev.key];
        if (!kc.normal)
            return false;

        bool shift = isPressed(SHIFT_LEFT) || isPressed(SHIFT_RIGHT);
        bool upper = capsLock_ ^ shift;
        out = upper ? kc.shifted : kc.normal;
        return true;
    }

    uint32_t getPs2AckCount() { return ps2AckCount_; }
    uint32_t getPs2WriteErrors() { return ps2WriteErrors_; }

} // namespace KEYBOARD