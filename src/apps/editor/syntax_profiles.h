#pragma once
#include <stdint.h>
#include <palette.h>
#include <cstring>

// Типы правил синтаксиса
enum class RuleType {
    KEYWORD,    // Ключевые слова (if, function, return)
    SEQUENCE,   // Последовательности (скобки, операторы)
    RANGE       // Диапазоны (строки, комментарии)
};

// Правило подсветки
struct SyntaxRule {
    RuleType type;
    const char* start;      // Начало токена (напр. "if" или "\"")
    const char* end;        // Конец токена (только для RANGE, напр. "\"" или "*/")
    uint8_t color;          // Цвет для этого правила
};

// Профиль языка
struct LanguageProfile {
    const char* extension;      // Расширение файла (".lua", ".cpp")
    const SyntaxRule* rules;    // Массив правил
    size_t ruleCount;           // Количество правил
};

// Цвета для подсветки (используем индексы палитры напрямую)
// Конвертация в реальные цвета происходит во время выполнения через getColorByPalette()
#define SYNTAX_COLOR_DEFAULT    COLOR_WHITE      // Белый (по умолчанию)
#define SYNTAX_COLOR_KEYWORD    COLOR_YELLOW     // Жёлтый (ключевые слова)
#define SYNTAX_COLOR_STRING     COLOR_GREEN      // Зелёный (строки)
#define SYNTAX_COLOR_COMMENT    COLOR_GRAY       // Тёмно-серый (комментарии)
#define SYNTAX_COLOR_NUMBER     COLOR_RED        // Красный (числа)
#define SYNTAX_COLOR_OPERATOR   COLOR_CYAN       // Бирюзовый (операторы)
#define SYNTAX_COLOR_BRACKET    COLOR_PURPLE     // Пурпурный (скобки)
#define SYNTAX_COLOR_FUNCTION   COLOR_BLUE       // Синий (функции)

// ============================================================
// Lua Profile
// ============================================================

const SyntaxRule LUA_RULES[] = {
    // Комментарии -- [...]
    { RuleType::RANGE,   "--[[", "]]",   SYNTAX_COLOR_COMMENT },
    { RuleType::RANGE,   "--", "\n",     SYNTAX_COLOR_COMMENT },
    
    // Строки
    { RuleType::RANGE,   "\"", "\"",     SYNTAX_COLOR_STRING },
    { RuleType::RANGE,   "'", "'",       SYNTAX_COLOR_STRING },
    
    // Ключевые слова
    { RuleType::KEYWORD, "function", "", SYNTAX_COLOR_FUNCTION },
    { RuleType::KEYWORD, "local", "",    SYNTAX_COLOR_KEYWORD },
    { RuleType::KEYWORD, "return", "",   SYNTAX_COLOR_KEYWORD },
    { RuleType::KEYWORD, "end", "",      SYNTAX_COLOR_KEYWORD },
    { RuleType::KEYWORD, "if", "",       SYNTAX_COLOR_KEYWORD },
    { RuleType::KEYWORD, "then", "",     SYNTAX_COLOR_KEYWORD },
    { RuleType::KEYWORD, "else", "",     SYNTAX_COLOR_KEYWORD },
    { RuleType::KEYWORD, "elseif", "",   SYNTAX_COLOR_KEYWORD },
    { RuleType::KEYWORD, "for", "",      SYNTAX_COLOR_KEYWORD },
    { RuleType::KEYWORD, "while", "",    SYNTAX_COLOR_KEYWORD },
    { RuleType::KEYWORD, "do", "",       SYNTAX_COLOR_KEYWORD },
    { RuleType::KEYWORD, "repeat", "",   SYNTAX_COLOR_KEYWORD },
    { RuleType::KEYWORD, "until", "",    SYNTAX_COLOR_KEYWORD },
    { RuleType::KEYWORD, "break", "",    SYNTAX_COLOR_KEYWORD },
    { RuleType::KEYWORD, "true", "",     SYNTAX_COLOR_NUMBER },
    { RuleType::KEYWORD, "false", "",    SYNTAX_COLOR_NUMBER },
    { RuleType::KEYWORD, "nil", "",      SYNTAX_COLOR_NUMBER },
    { RuleType::KEYWORD, "and", "",      SYNTAX_COLOR_OPERATOR },
    { RuleType::KEYWORD, "or", "",       SYNTAX_COLOR_OPERATOR },
    { RuleType::KEYWORD, "not", "",      SYNTAX_COLOR_OPERATOR },
    
    // Скобки
    { RuleType::SEQUENCE, "(", "",       SYNTAX_COLOR_BRACKET },
    { RuleType::SEQUENCE, ")", "",       SYNTAX_COLOR_BRACKET },
    { RuleType::SEQUENCE, "{", "",       SYNTAX_COLOR_BRACKET },
    { RuleType::SEQUENCE, "}", "",       SYNTAX_COLOR_BRACKET },
    { RuleType::SEQUENCE, "[", "",       SYNTAX_COLOR_BRACKET },
    { RuleType::SEQUENCE, "]", "",       SYNTAX_COLOR_BRACKET },
};

// ============================================================
// JSON Profile
// ============================================================

const SyntaxRule JSON_RULES[] = {
    // Строки
    { RuleType::RANGE,   "\"", "\"",     SYNTAX_COLOR_STRING },

    // Числа и булевы
    { RuleType::KEYWORD, "true", "",     SYNTAX_COLOR_NUMBER },
    { RuleType::KEYWORD, "false", "",    SYNTAX_COLOR_NUMBER },
    { RuleType::KEYWORD, "null", "",     SYNTAX_COLOR_NUMBER },

    // Скобки и разделители
    { RuleType::SEQUENCE, "{", "",       SYNTAX_COLOR_BRACKET },
    { RuleType::SEQUENCE, "}", "",       SYNTAX_COLOR_BRACKET },
    { RuleType::SEQUENCE, "[", "",       SYNTAX_COLOR_BRACKET },
    { RuleType::SEQUENCE, "]", "",       SYNTAX_COLOR_BRACKET },
    { RuleType::SEQUENCE, ":", "",       SYNTAX_COLOR_OPERATOR },
    { RuleType::SEQUENCE, ",", "",       SYNTAX_COLOR_OPERATOR },
};

// ============================================================
// Профили языков
// ============================================================

const LanguageProfile PROFILES[] = {
    { ".lua",  LUA_RULES,  sizeof(LUA_RULES)  / sizeof(LUA_RULES[0]) },
    { ".json", JSON_RULES, sizeof(JSON_RULES) / sizeof(JSON_RULES[0]) },
    { ".txt",  nullptr,    0 },
    { nullptr, nullptr,    0 }  // Терминатор
};

// ============================================================
// Вспомогательные функции
// ============================================================

// Проверка, является ли символ буквой/цифрой/подчёркиванием
inline bool isWordChar(char c) {
    return (c >= 'a' && c <= 'z') || 
           (c >= 'A' && c <= 'Z') || 
           (c >= '0' && c <= '9') || 
           (c == '_');
}

// Проверка ключевого слова (целое слово, не часть другого)
inline bool isKeyword(const char* text, const char* keyword) {
    size_t len = 0;
    while (keyword[len]) {
        if (text[len] != keyword[len]) return false;
        len++;
    }
    // Проверяем, что после ключевого слова не идёт буква/цифра
    return !isWordChar(text[len]);
}

// Получить профиль для файла по расширению
inline const LanguageProfile* getProfileForFile(const char* filename) {
    if (!filename) return nullptr;
    
    // Ищем точку в имени файла
    const char* dot = nullptr;
    for (const char* p = filename; *p; p++) {
        if (*p == '.') dot = p;
    }
    
    if (!dot) return &PROFILES[5];  // .txt по умолчанию
    
    // Ищем профиль по расширению
    for (size_t i = 0; PROFILES[i].extension; i++) {
        if (strcmp(dot, PROFILES[i].extension) == 0) {
            return &PROFILES[i];
        }
    }
    
    return &PROFILES[5];  // .txt по умолчанию
}
