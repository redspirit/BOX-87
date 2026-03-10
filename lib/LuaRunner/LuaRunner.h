#pragma once
#include <stdint.h>
extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
    #include "lualib.h"
}

// Типы функций для консольного вывода (function pointers)
typedef void (*LuaPrintFunc)(void* userData, const char* text);
typedef void (*LuaPrintLnFunc)(void* userData);
typedef void (*LuaSetColorFunc)(void* userData, uint8_t color);
typedef void (*LuaUseDefaultColorFunc)(void* userData);

// Структура с callbacks для консоли
struct LuaConsoleCallbacks {
    void* userData;
    LuaPrintFunc print;
    LuaPrintLnFunc printLn;
    LuaSetColorFunc setColorRaw;
    LuaUseDefaultColorFunc useDefaultColor;
};

// Класс для запуска Lua кода
class LuaRunner {
public:
    LuaRunner();
    ~LuaRunner();

    // Инициализация Lua state
    bool init(LuaConsoleCallbacks* callbacks);

    // Загрузка и выполнение Lua кода из буфера (память)
    bool loadFromBuffer(const char* code, size_t len);

    // Загрузка и выполнение Lua кода из файла (SD-карта)
    bool loadFromFile(const char* path);

    // Установка аргументов командной строки (arg table)
    void setArguments(int argc, const char** argv);

    // Вызов функции main() из Lua кода
    bool callMain();

    // Тик (для длительных операций, пока пусто)
    void tick();

    // Отмена выполнения
    void cancel();

    // Проверка завершения
    bool isFinished() const;

    // Получение Lua state (для расширенных операций)
    lua_State* getState() { return L; }

private:
    lua_State* L;
    LuaConsoleCallbacks* _callbacks;
    bool _finished;

    // Регистрация binding'ов (console.print и т.д.)
    void registerBindings();

    // Lua reader для загрузки из SD-карты
    static const char* luaSDReader(lua_State* L, void* data, size_t* size);

    // Временный буфер для чтения из SD
    char _luaBuffer[512];
    const char* _currentPath;
};
