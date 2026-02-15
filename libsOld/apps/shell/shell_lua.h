#pragma once
#include <stddef.h>

class ShellLua {
    public:
        bool runExpression(const char* expr, char* out, size_t outSize);
};