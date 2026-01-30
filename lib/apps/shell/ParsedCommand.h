#pragma once

#include <stdint.h>

#define SHELL_MAX_ARGS     8
#define SHELL_ARG_LEN      32

/*
 * Результат парсинга команды
 * Аналог argv/argc в POSIX
 */
class ParsedCommand {
    public:
        ParsedCommand();

        void clear();

        int argc;
        char argv[SHELL_MAX_ARGS][SHELL_ARG_LEN];
};

/*
 * Парсер строки shell-команды
 * Возвращает false если строка пустая или ошибка
 */
bool parseCommand(const char* line, ParsedCommand& out);
