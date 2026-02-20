#pragma once
#include <stdint.h>
#include <stddef.h>

namespace UTF8 {

    /**
     * @brief Декодирует следующий символ из UTF-8 потока.
     * @param text Указатель на текущий байт строки (обновляется внутри).
     * @param code Ссылка, куда будет записан полученный Unicode код.
     * @return const char* Указатель на начало следующего символа.
     */
    static inline const char* decode(const char* text, uint16_t& code) {
        if (!text || !*text) {
            code = 0;
            return nullptr;
        }

        uint8_t c = (uint8_t)*text++;

        // 1. ASCII и управляющие символы (0xxxxxxx)
        if (c < 0x80) {
            code = c;
            return text;
        }

        // 2. Двухбайтовые последовательности (110xxxxx 10xxxxxx)
        // Примеры: Кириллица, Греческий
        if ((c & 0xE0) == 0xC0) {
            if ((*text & 0xC0) != 0x80) { code = '?'; return text; }
            code = (c & 0x1F) << 6;
            code |= (*text++ & 0x3F);
            return text;
        }

        // 3. Трехбайтовые последовательности (1110xxxx 10xxxxxx 10xxxxxx)
        // Примеры: Псевдографика, Иероглифы
        if ((c & 0xF0) == 0xE0) {
            if ((text[0] & 0xC0) != 0x80 || (text[1] & 0xC0) != 0x80) { 
                code = '?'; return text + 2; 
            }
            code = (c & 0x0F) << 12;
            code |= (*text++ & 0x3F) << 6;
            code |= (*text++ & 0x3F);
            return text;
        }

        // 4. Четырехбайтовые последовательности (11110xxx ...)
        // Выходят за пределы uint16_t (Basic Multilingual Plane).
        if ((c & 0xF8) == 0xF0) {
            code = 0xFFFD; // Unicode Replacement Character
            return text + 3;
        }

        code = '?';
        return text;
    }

    //Считает количество символов в UTF-8 строке (не байт!).
    static inline size_t length(const char* text) {
        size_t len = 0;
        uint16_t dummy;
        const char* ptr = text;
        while (ptr && *ptr) {
            ptr = decode(ptr, dummy);
            len++;
        }
        return len;
    }

    //Проверяет, является ли символ управляющим (\r или \n).
    static inline bool isControl(uint16_t code) {
        return (code == '\r' || code == '\n');
    }

} // namespace UTF8