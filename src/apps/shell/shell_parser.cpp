#include "shell_parser.h"
#include <string.h>

/* ==== ParsedCommand ==== */

ShellParser::ShellParser() {
    clear();
}

void ShellParser::clear() {
    argc = 0;
    for (int i = 0; i < SHELL_MAX_ARGS; ++i)
        argv[i][0] = 0;
}

/* ==== PARSER ==== */

bool parseCommand(const char* line, ShellParser& out) {
    out.clear();

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

        if (!inQuote && (c == ' ' || c == '\t')) {
            if (argPos > 0) {
                out.argv[out.argc][argPos] = 0;
                out.argc++;
                if (out.argc >= SHELL_MAX_ARGS)
                    return true;
                argPos = 0;
            }
            continue;
        }

        if (argPos < SHELL_ARG_LEN - 1) {
            out.argv[out.argc][argPos++] = c;
        }
    }

    if (argPos > 0 && out.argc < SHELL_MAX_ARGS) {
        out.argv[out.argc][argPos] = 0;
        out.argc++;
    }

    return out.argc > 0;
}