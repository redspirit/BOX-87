#pragma once

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

class CmdParser {
public:
    // Константы можно вынести в public или оставить в private
    static constexpr int MAX_ARGS = 8;
    static constexpr int MAX_ARG_LEN = 32;

    CmdParser() {
        clear();
    }

    /**
     * Основная логика парсинга строки
     */
    bool parse(const char* line) {
        clear();

        if (!line || !line[0])
            return false;

        bool inQuote = false;
        int argPos = 0;

        while (*line) {
            char c = *line++;

            if (c == '"') {
                inQuote = !inQuote;
                continue;
            }

            // Разделители: пробел, табуляция, переносы строк
            if (!inQuote && (c == ' ' || c == '\t' || c == '\r' || c == '\n')) {
                if (argPos > 0) {
                    finalizeArgument(argPos);
                    if (_argc >= MAX_ARGS) return true;
                    argPos = 0;
                }
                continue;
            }

            // Заполнение текущего аргумента
            if (argPos < MAX_ARG_LEN - 1) {
                _argv[_argc][argPos++] = c;
            }
        }

        // Обработка последнего аргумента, если строка не закончилась разделителем
        if (argPos > 0 && _argc < MAX_ARGS) {
            finalizeArgument(argPos);
        }

        return _argc > 0;
    }

    // --- Методы доступа ---

    int argc() const { 
        return _argc; 
    }
    
    const char* argv(int index) const {
        if (index >= 0 && index < _argc) {
            return _argv[index];
        }
        return "";
    }

    /**
     * Проверка: совпадает ли имя команды (нулевой аргумент)
     */
    bool is(const char* commandName) const {
        if (_argc == 0) return false;
        return strcasecmp(_argv[0], commandName) == 0;
    }

    int intArg(int index) const {
        const char* val = argv(index);
        return (val[0] != '\0') ? atoi(val) : 0;
    }

    float floatArg(int index) const {
        const char* val = argv(index);
        return (val[0] != '\0') ? (float)atof(val) : 0.0f;
    }

    /**
     * Очистка состояния
     */
    void clear() {
        _argc = 0;
        for (int i = 0; i < MAX_ARGS; ++i) {
            _argv[i][0] = '\0';
        }
    }

private:
    int _argc = 0;
    char _argv[MAX_ARGS][MAX_ARG_LEN];

    // Вспомогательный метод для закрытия строки аргумента
    void finalizeArgument(int& pos) {
        _argv[_argc][pos] = '\0';
        _argc++;
    }
};