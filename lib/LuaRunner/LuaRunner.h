#pragma once
#include <stdint.h>
extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
    #include "lualib.h"
}

// Интерфейс для консольного вывода (абстракция)
class ILuaConsole {
public:
    virtual ~ILuaConsole() = default;
    
    virtual void print(const char* text) = 0;
    virtual void printLn() = 0;
    virtual void setColorRaw(uint8_t color) = 0;
    virtual void useDefaultColor() = 0;
};

// Класс для запуска Lua кода
class LuaRunner {
public:
    LuaRunner();
    ~LuaRunner();
    
    // Инициализация Lua state
    bool init(ILuaConsole* console);
    
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
    ILuaConsole* _console;
    bool _finished;
    
    // Регистрация binding'ов (console.print и т.д.)
    void registerBindings();
    
    // Lua reader для загрузки из SD-карты
    static const char* luaSDReader(lua_State* L, void* data, size_t* size);
    
    // Временный буфер для чтения из SD
    char _luaBuffer[512];
    const char* _currentPath;
};
