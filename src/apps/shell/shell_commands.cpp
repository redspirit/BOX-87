#include "shell_commands.h"
#include "palette.h"
#include "shell.h"
#include "CmdPing.h"
#include "apps/helloworld/helloworld.h"
#include "apps/viewer/viewer.h"
// #include "player/player.h"
#include "AppManager.h"
#include "sdcard.h"
#include "OTBFont.h"
#include <LittleFS.h>

#include <string.h>
#include <stdio.h>
#include <esp_system.h>

#define TYPE_MAX_SIZE 1024
#define SDSPEED_BUFFER  (32 * 1024)  // 32 KB


static IShellCommand* create_ping() {
    return new CmdPing();
}

/* =========================================================
 *  COMMAND HANDLERS
 * ========================================================= */

static bool cmd_cls(Shell& shell);
static bool cmd_help(Shell& shell);
static bool cmd_reboot(Shell& shell);
static bool cmd_font(Shell& shell);
static bool cmd_palette(Shell& shell);
static bool cmd_pwd(Shell& shell);
static bool cmd_cd(Shell& shell);
static bool cmd_dir(Shell& shell);
static bool cmd_type(Shell& shell);
static bool cmd_mkdir(Shell& shell);
static bool cmd_rd(Shell& shell);
static bool cmd_del(Shell& shell);
static bool cmd_write(Shell& shell);
static bool cmd_append(Shell& shell);
static bool cmd_lua(Shell& shell);
static bool cmd_mem(Shell& shell);
static bool cmd_hw(Shell& shell);
static bool cmd_play(Shell& shell);
static bool cmd_pcm(Shell& shell);
static bool cmd_color(Shell& shell);
static bool cmd_sdspeed(Shell& shell);
static bool cmd_glyph(Shell& shell);
static bool cmd_fs(Shell& shell);
static bool cmd_view(Shell& shell);

/* =========================================================
 *  COMMAND TABLE
 * ========================================================= */

struct ShellCommand {
    const char* name;
    bool (*handler)(Shell&);
    IShellCommand* (*create)();
    const char* help;
};

static const ShellCommand commands[] = {
    { "HELP",  cmd_help,  nullptr, "Get this help" },
    { "CLS",  cmd_cls,  nullptr, "Clear screen" },
    { "REBOOT", cmd_reboot, nullptr, "Restart system" },
    { "FONT",   cmd_font,  nullptr, "Show font table" },
    { "PALETTE",  cmd_palette, nullptr, "Show color palette" },
    { "PWD",  cmd_pwd, nullptr, "Show current directory" },
    { "CD",  cmd_cd, nullptr, "Change current directory" },
    { "DIR",  cmd_dir, nullptr, "List directory contents" },
    { "TYPE",  cmd_type, nullptr, "Display text file" },
    { "RD",    cmd_rd,  nullptr,  "Remove empty directory" },
    { "DEL",   cmd_del, nullptr,  "Delete file" },
    { "MKDIR", cmd_mkdir, nullptr, "Create directory" },
    { "WRITE",  cmd_write, nullptr, "Write text file or create empty" },
    { "APPEND", cmd_append, nullptr, "Append text to file" },
    { "LUA", cmd_lua, nullptr, "Execute Lua expression" },
    { "MEM", cmd_mem, nullptr, "Show memory info" },
    { "HW", cmd_hw, nullptr, "Start Hello World application" },
    { "PLAY", cmd_play, nullptr, "Play media file" },
    { "PCM", cmd_pcm, nullptr, "Play pcm audio file" },
    { "COLOR", cmd_color, nullptr, "Show specific color" },
    { "SDSPEED", cmd_sdspeed, nullptr, "Test speed of file read from sd" },
    { "GLYPH", cmd_glyph, nullptr, "Show glyph bitmap for current font" },
    { "FS", cmd_fs, nullptr, "FS" },
    { "VIEW", cmd_view, nullptr, "View image" },
    { "PING", nullptr, create_ping, "Test log command" },
};

static const int commandCount = sizeof(commands) / sizeof(commands[0]);

static char hexDigit(uint8_t v) {
    return (v < 10) ? ('0' + v) : ('A' + (v - 10));
}

void printHexU16(Console& con, uint16_t value) {
    const char hexChars[] = "0123456789ABCDEF";
    con.print("U+");
    con.printRawChar(hexChars[(value >> 12) & 0xF]);
    con.printRawChar(hexChars[(value >> 8) & 0xF]);
    con.printRawChar(hexChars[(value >> 4) & 0xF]);
    con.printRawChar(hexChars[value & 0xF]);
    con.print(" ");
}

static void dirCallback(void* user, const char* name, bool isDir) {
    Shell* shell = static_cast<Shell*>(user);
    auto& con = shell->console();

    if (isDir) {
        con.setColor(COLOR_YELLOW);
        con.print("<D> ");
    } else {
        con.print("    ");
    }

    con.setColor(COLOR_WHITE);
    con.printLn(name);
}

static bool sdCheck(Shell& shell) {
    if (!SDCARD::init()) {
        shell.console().setColor(COLOR_RED);
        shell.console().printLn("SD card not initialized");
        shell.console().useDefaultColor();
        return false;
    }
    return true;
}

/* =========================================================
 *  EXECUTOR
 * ========================================================= */

bool shellExecute(Shell& shell) {
    auto& con = shell.console();
    auto& cmd = shell.parsedCmd();
    
    if (cmd.argc() == 0)
        return false;

    for (int i = 0; i < commandCount; ++i) {
        if (cmd.is(commands[i].name)) {
            if (commands[i].handler == nullptr) {

                if (shell.hasActiveCommand()) {
                    con.printLn("Another command already running");
                    return false;
                }

                IShellCommand* c = commands[i].create();
                if (!c)
                    return false;

                shell.setActiveCommand(c);
                c->start(shell);

                return true;                

            } 

            return commands[i].handler(shell);
        }
    }

    con.setColor(COLOR_RED);
    con.print("Unknown command: ");
    con.print(cmd.argv(0));
    con.useDefaultColor();
    con.printLn();

    return false;
}

/* =========================================================
 *  COMMAND IMPLEMENTATIONS
 * ========================================================= */

static bool cmd_help(Shell& shell) {
    auto& con = shell.console();
    auto& cmd = shell.parsedCmd();

    if (cmd.argc() == 2) {
        for (int i = 0; i < commandCount; ++i) {
            if (strcasecmp(cmd.argv(1), commands[i].name) == 0) {

                con.setColor(COLOR_YELLOW);
                con.print(commands[i].name);
                con.setColor(COLOR_WHITE);
                con.print(" - ");
                con.printLn(commands[i].help);

                return true;
            }
        }

        // команда не найдена
        con.setColor(COLOR_RED);
        con.print("No help available for command: ");
        con.printLn(cmd.argv(1));
        con.setColor(COLOR_WHITE);
        return false;
    }

    con.setColor(COLOR_CYAN);
    con.printLn("Available commands:");
    con.printLn("-------------------");
    con.setColor(COLOR_WHITE);

    // ищем максимальную длину имени команды
    int maxLen = 0;
    for (int i = 0; i < commandCount; ++i) {
        int len = strlen(commands[i].name);
        if (len > maxLen)
            maxLen = len;
    }

    // печатаем список
    for (int i = 0; i < commandCount; ++i) {
        con.setColor(COLOR_YELLOW);
        con.print(commands[i].name);
        con.setColor(COLOR_WHITE);

        int pad = maxLen - strlen(commands[i].name) + 2;
        for (int s = 0; s < pad; ++s)
            con.printRawChar(' ');

        con.printLn(commands[i].help);
    }

    return true;
}

static bool cmd_cls(Shell& shell) {
    shell.console().clear();
    return true;
}

static bool cmd_reboot(Shell& shell) {
    shell.console().printLn("Rebooting...");
    esp_restart();
    return true;
}

static bool cmd_font(Shell& shell) {
    auto& con = shell.console();
    auto& font = con.tiles().getFont();

    con.setColor(COLOR_CYAN);
    con.printLn("FULL FONT TABLE");
    con.printLn("==============================");

    // Переменная для отслеживания позиции в строке
    int count = 0; 

    font.forEachUnicode([&](uint16_t code) {
        // Если это начало строки (каждые 16 символов)
        if (count % 32 == 0) {
            if (count > 0) con.printLn(); // Перенос предыдущей строки
            
            con.setColor(COLOR_YELLOW);
            printHexU16(con, code); // Выводим "U+XXXX "
            con.setColor(COLOR_WHITE);
        }

        con.printRawChar(code);
        count++;
    });

    con.printLn();
    con.printLn();
    con.setColor(COLOR_CYAN);
    con.print("Total symbols found: ");
    
    // Преобразование итогового числа в строку (простой вариант)
    char totalStr[10];
    itoa(count, totalStr, 10);
    con.printLn(totalStr);
    
    con.useDefaultColor();
    return true;
}

static bool cmd_palette(Shell& shell) {
    auto& con = shell.console();

    con.setColor(COLOR_CYAN);
    con.printLn("COLOR PALETTE (0x00-0xFF)");
    con.printRawChar((char)205, 24);
    con.printLn();

    for (int base = 0; base < 256; base += 16) {
        // Заголовок строки: 0xNN
        con.setColor(COLOR_YELLOW);
        con.print("0x");
        con.printRawChar(hexDigit((base >> 4) & 0xF));
        con.printRawChar(hexDigit(base & 0xF));
        con.print(" ");

        // 16 цветов
        for (int i = 0; i < 16; i++) {
            uint8_t color = base + i;
            con.setColor(color);
            con.print("█");
        }

        con.printLn();
    }
    con.useDefaultColor();
    return true;
}

static bool cmd_pwd(Shell& shell) {
    auto& con = shell.console();
    con.setColor(COLOR_YELLOW);
    con.printLn(shell.cwd());
    con.useDefaultColor();
    return true;
}

static bool cmd_cd(Shell& shell) {
    auto& con = shell.console();
    auto& cmd = shell.parsedCmd();

    if (cmd.argc() < 2) {
        shell.setCwd("/");
        return true;
    }

    char newPath[MAX_PATH];
    shell.resolvePath(cmd.argv(1), newPath);

    if (!SDCARD::dirExists(newPath)) {
        con.setColor(COLOR_RED);
        con.print("Directory not found: ");
        con.printLn(newPath);
        con.useDefaultColor();;
        return false;
    }

    shell.setCwd(newPath);

    return true;
}

static bool cmd_dir(Shell& shell) {
    auto& con = shell.console();
    auto& cmd = shell.parsedCmd();

    char path[MAX_PATH];

    // DIR или DIR <path>
    if (cmd.argc() > 1) {
        shell.resolvePath(cmd.argv(1), path);
    } else {
        strncpy(path, shell.cwd(), MAX_PATH);
        path[MAX_PATH - 1] = 0;
    }

    if (!sdCheck(shell)) {
        return false;
    }

    // проверка, что это директория
    if (!SDCARD::dirExists(path)) {
        con.setColor(COLOR_RED);
        con.print("Directory not found: ");
        con.printLn(path);
        con.useDefaultColor();
        return false;
    }

    SDCARD::listDir(path, dirCallback, &shell);

    return true;
}

static bool cmd_type(Shell& shell) {
    auto& con = shell.console();
    auto& cmd = shell.parsedCmd();

    if (cmd.argc() < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: TYPE <file>");
        con.useDefaultColor();
        return false;
    }

    char path[MAX_PATH];
    shell.resolvePath(cmd.argv(1), path);

    if (!sdCheck(shell)) {
        return false;
    }

    char buffer[TYPE_MAX_SIZE + 1];

    if (!SDCARD::readTextFileLimited(path, buffer, TYPE_MAX_SIZE)) {
        con.setColor(COLOR_RED);
        con.printLn("File not found or too large (max 1 KB)");
        con.useDefaultColor();
        return false;
    }

    con.printLn("");
    con.printLn(buffer);    

    return true;
}

static bool cmd_mkdir(Shell& shell) {
    auto& con = shell.console();
    auto& cmd = shell.parsedCmd();

    if (cmd.argc() < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: MKDIR <dir>");
        con.useDefaultColor();
        return false;
    }

    char path[MAX_PATH];
    shell.resolvePath(cmd.argv(1), path);

    if (!sdCheck(shell)) {
        return false;
    }

    if (!SDCARD::mkdir(path)) {
        con.setColor(COLOR_RED);
        con.print("Cannot create directory: ");
        con.printLn(path);
        con.useDefaultColor();
        return false;
    } 

    return true;
}

static bool cmd_rd(Shell& shell) {
    auto& con = shell.console();
    auto& cmd = shell.parsedCmd();

    if (cmd.argc() < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: RD <dir>");
        con.useDefaultColor();
        return false;
    }

    char path[MAX_PATH];
    shell.resolvePath(cmd.argv(1), path);

    if (!sdCheck(shell)) {
        return false;
    }

    if (!SDCARD::rmdirEmpty(path)) {
        con.setColor(COLOR_RED);
        con.print("Cannot remove directory (not empty?): ");
        con.printLn(path);
        con.useDefaultColor();
        return false;
    }

    return true;
}

static bool cmd_del(Shell& shell) {
    auto& con = shell.console();
    auto& cmd = shell.parsedCmd();

    if (cmd.argc() < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: DEL <file>");
        con.useDefaultColor();
        return false;
    }

    char path[MAX_PATH];
    shell.resolvePath(cmd.argv(1), path);

    if (!sdCheck(shell)) {
        return false;
    }

    if (!SDCARD::removeFile(path)) {
        con.setColor(COLOR_RED);
        con.print("Cannot delete file: ");
        con.printLn(path);
        con.useDefaultColor();
        return false;
    }

    return true;
}

static bool cmd_write(Shell& shell) {
    auto& con = shell.console();
    auto& cmd = shell.parsedCmd();

    if (cmd.argc() < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: WRITE <file> [text]");
        con.useDefaultColor();
        return false;
    }

    char path[MAX_PATH];
    shell.resolvePath(cmd.argv(1), path);

    // const char* text = getTextArg(cmd);
    const char* text = "TEST STRING"; // todo тут надо вычленять строку из команды, пока это не работает

    if (!sdCheck(shell)) {
        return false;
    }

    if (!SDCARD::writeTextFile(path, text)) {
        con.setColor(COLOR_RED);
        con.print("Cannot write file: ");
        con.printLn(path);
        con.useDefaultColor();
        return false;
    }

    return true;
}

static bool cmd_append(Shell& shell) {
    auto& con = shell.console();
    auto& cmd = shell.parsedCmd();

    if (cmd.argc() < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: APPEND <file> <text>");
        con.useDefaultColor();
        return false;
    }

    char path[MAX_PATH];
    shell.resolvePath(cmd.argv(1), path);

    const char* text = "TEST STRING";

    if (!sdCheck(shell)) {
        return false;
    }

    if (!SDCARD::appendTextFile(path, text)) {
        con.setColor(COLOR_RED);
        con.print("Cannot append file: ");
        con.printLn(path);
        con.useDefaultColor();
        return false;
    }

    return true;
}

static bool cmd_sdspeed(Shell& shell) {
    auto& con = shell.console();
    auto& cmd = shell.parsedCmd();

    if (cmd.argc() < 1) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: SDSPEED <file>");
        con.useDefaultColor();
        return false;
    }

    char path[MAX_PATH];
    shell.resolvePath(cmd.argv(1), path);

    if (!sdCheck(shell)) {
        return false;
    }

    size_t fileSize = SDCARD::fileSize(path);
    if (fileSize == 0) {
        con.setColor(COLOR_RED);
        con.printLn("File not found or empty");
        con.useDefaultColor();        
        return false;
    }

    if (!SDCARD::open(path)) {
        con.setColor(COLOR_RED);
        con.printLn("Failed to open file");
        con.useDefaultColor();          
        return false;
    }

    uint8_t* buffer = (uint8_t*)malloc(SDSPEED_BUFFER);
    if (!buffer) {
        con.setColor(COLOR_RED);        
        con.printLn("Out of memory!");
        con.useDefaultColor();
        SDCARD::close();
        return false;
    }

    uint64_t totalRead = 0;
    uint32_t startTime = millis();
    uint32_t lastReport = startTime;
    uint64_t lastReadBytes = 0;

    con.printLn("Starting SD speed test...");
    con.printLn();

    while (SDCARD::available()) {
        size_t toRead = SDSPEED_BUFFER;
        if (fileSize - totalRead < SDSPEED_BUFFER)
            toRead = fileSize - totalRead;

        size_t read = SDCARD::read(buffer, toRead);
        if (read == 0)
            break;

        totalRead += read;

        uint32_t now = millis();

        // Раз в секунду выводим статистику
        if (now - lastReport >= 1000) {

            uint32_t interval = now - lastReport;
            uint64_t intervalBytes = totalRead - lastReadBytes;
            uint32_t speedKB = (intervalBytes / 1024) * 1000 / interval;
            uint32_t percent = (totalRead * 100ULL) / fileSize;

            con.printInt((int)percent); con.print("%  ");
            con.printInt((int)totalRead); con.print(" bytes ");
            con.printInt((int)speedKB); con.printLn(" KB/s");

            lastReport = now;
            lastReadBytes = totalRead;
        }
        delay(0);
    }

    uint32_t endTime = millis();
    uint32_t totalTime = endTime - startTime;

    SDCARD::close();
    free(buffer);

    if (totalTime == 0) totalTime = 1;

    uint32_t avgKB = (totalRead / 1024) * 1000 / totalTime;

    con.printLn();
    con.printLn("=== SD SPEED SUMMARY ===");
    con.print("File size: "); con.printInt(fileSize); con.printLn();
    con.print("Time: "); con.printInt((int)totalTime); con.printLn(" ms");
    con.print("Average speed: "); con.printInt((int)avgKB); con.printLn(" KB/s");    

    return true;
}

static bool cmd_mem(Shell& shell) {
    auto& con = shell.console();

    // ---------- Internal RAM ----------
    uint32_t ram_total = heap_caps_get_total_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    uint32_t ram_free  = heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    uint32_t ram_min   = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);

    // ---------- PSRAM ----------
    uint32_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    uint32_t psram_free  = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    uint32_t psram_min   = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);

    con.printLn("INTERNAL RAM");
    con.print("  TOTAL: ");
    con.printInt((int)(ram_total >> 10));
    con.printLn(" KB");

    con.print("  FREE : ");
    con.printInt((int)(ram_free >> 10));
    con.printLn(" KB");

    con.print("  USED : ");
    con.printInt((int)((ram_total - ram_free) >> 10));
    con.printLn(" KB");

    con.print("  MIN  : ");
    con.printInt((int)(ram_min >> 10));
    con.printLn(" KB");

    if (psram_total > 0) {
        con.printLn("");
        con.printLn("PSRAM");

        con.print("  TOTAL: ");
        con.printInt((int)(psram_total >> 10));
        con.printLn(" KB");

        con.print("  FREE : ");
        con.printInt((int)(psram_free >> 10));
        con.printLn(" KB");

        con.print("  USED : ");
        con.printInt((int)((psram_total - psram_free) >> 10));
        con.printLn(" KB");

        con.print("  MIN  : ");
        con.printInt((int)(psram_min >> 10));
        con.printLn(" KB");
    } else {
        con.printLn("");
        con.printLn("PSRAM: NOT PRESENT");
    }    

    return true;
}

static bool cmd_lua(Shell& shell) {
    auto& con = shell.console();
    auto& cmd = shell.parsedCmd();

    if (cmd.argc() < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: LUA <expression>");
        con.useDefaultColor();
        return false;
    }

    // всё после LUA — выражение
    const char* expr = cmd.argv(1);

    char out[128];

    if (!shell.lua().runExpression(expr, out, sizeof(out))) {
        con.setColor(COLOR_RED);
        con.printLn(out[0] ? out : "Lua error");
        con.useDefaultColor();
        return false;
    }

    con.printLn(out);

    return true;
}

static bool cmd_hw(Shell& shell) {
    shell.app().requestSwitch(
        new HelloWorld()
    );
    return true;
}

static bool cmd_play(Shell& shell) {
    auto& con = shell.console();
    auto& cmd = shell.parsedCmd();

    if (cmd.argc() < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: PLAY <file>");
        con.useDefaultColor();
        return false;
    }

    char path[MAX_PATH];
    shell.resolvePath(cmd.argv(1), path);

    if (!sdCheck(shell)) {
        return false;
    }

    if (!SDCARD::fileExists(path)) {
        con.setColor(COLOR_RED);
        con.print("File not found: ");
        con.printLn(path);
        con.useDefaultColor();
        return false;
    }

    // shell.app().requestSwitch(
    //     new Player(cmd, path)
    // ); 

    return true;
}

static bool cmd_pcm(Shell& shell) {
    auto& con = shell.console();
    auto& cmd = shell.parsedCmd();

    if (cmd.argc() < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: PCM <file>");
        con.useDefaultColor();
        return false;
    }

    char path[MAX_PATH];
    shell.resolvePath(cmd.argv(1), path);

    if (!sdCheck(shell)) {
        return false;
    }

    // if (!SDCARD::fileExists(path)) {
    //     con.setColor(COLOR_RED);
    //     con.print("File not found: ");
    //     con.printLn(path);
    //     con.useDefaultColor();
    //     return false;
    // }

    // shell.app().requestSwitch(
    //     new Audio(path)
    // ); 

    return true;
}

static bool cmd_color(Shell& shell) {
    auto& con = shell.console();
    auto& cmd = shell.parsedCmd();

    if (cmd.argc() < 3) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: COLOR BIN 11111111");
        con.printLn("Usage: COLOR HEX FF");
        con.printLn("Usage: COLOR RGB <R> <G> <B>");      
        con.printLn("Usage: COLOR GRADIENT BLUE");      
        con.useDefaultColor();
        return false;
    }
    const char* mode  = cmd.argv(1);
    const char* value = cmd.argv(2);

    uint8_t color = 0;

    if (!strcasecmp(mode, "BIN")) {
        size_t len = strlen(value);
        if (len == 0 || len > 8) {
            con.setColor(COLOR_RED);
            con.printLn("BIT value must be 1..8 bits");
            con.useDefaultColor();
            return false;
        }

        for (size_t i = 0; i < len; ++i) {
            char c = value[i];
            if (c != '0' && c != '1') {
                con.setColor(COLOR_RED);
                con.printLn("BIT value must contain only 0 or 1");
                con.useDefaultColor();
                return false;
            }
            color = (color << 1) | (c - '0');
        }
    } else if (!strcasecmp(mode, "HEX")) {
        char* end = nullptr;
        unsigned long v = strtoul(value, &end, 16);
        if (*end != 0 || v > 0xFF) {
            con.setColor(COLOR_RED);
            con.printLn("HEX value must be 00..FF");
            con.useDefaultColor();
            return false;
        }
        color = (uint8_t)v;
    } else if (!strcasecmp(mode, "RGB")) {
        if (cmd.argc() < 5) {
            con.setColor(COLOR_RED);
            con.printLn("Usage: COLOR RGB <R> <G> <B>");
            con.useDefaultColor();
            return false;
        }

        auto parseByte = [&](const char* s, uint8_t& out) -> bool {
            char* end = nullptr;
            long v = strtol(s, &end, 10);
            if (*end != 0 || v < 0 || v > 255)
                return false;
            out = (uint8_t)v;
            return true;
        };

        uint8_t r, g, b;
        if (!parseByte(cmd.argv(2), r) ||
            !parseByte(cmd.argv(3), g) ||
            !parseByte(cmd.argv(4), b)) {
            con.setColor(COLOR_RED);
            con.printLn("RGB values must be 0..255");
            con.useDefaultColor();
            return false;
        }

        color = rgb332(r, g, b);
    } else if (!strcasecmp(mode, "GRADIENT")) {

        // blue gradient
        for (int cv = 0; cv < 4; cv++) {
            con.setColorRaw(cv << 6);
            con.printRawChar(u'█', 20);
            con.useDefaultColor();
            con.printInt(cv);
            con.printLn();
        }        
        
        // green gradient
        for (int cv = 0; cv < 8; cv++) {
            con.setColorRaw(cv << 3);
            con.printRawChar(u'█', 20);
            con.useDefaultColor();
            con.printInt(cv);
            con.printLn();
        }

        // red gradient
        for (int cv = 0; cv < 8; cv++) {
            con.setColorRaw(cv);
            con.printRawChar(u'█', 20);
            con.useDefaultColor();
            con.printInt(cv);
            con.printLn();
        }

        con.useDefaultColor();
        return true;
    } else {
        con.setColor(COLOR_RED);
        con.printLn("Unknown format (use BIN or HEX)");
        con.useDefaultColor();
        return false;
    }

    con.setColorRaw(color);

    for (int y = 0; y < 5; ++y) {
        con.printRawChar((char)219, 10);
        con.printLn();
    }

    con.useDefaultColor();
    return true;
}

static bool cmd_glyph(Shell& shell) {
    auto& con = shell.console();
    auto& cmd = shell.parsedCmd();
    auto& tiles = shell.console().tiles();

    if (cmd.argc() < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: GLYPH <char>");
        con.useDefaultColor();
        return false;
    }

    if (!sdCheck(shell)) {
        return false;
    }

    const char* ch  = cmd.argv(1);
    int value = atoi(ch);

    char buffer[TYPE_MAX_SIZE + 1];

    if(!SDCARD::open("/fonts/ToshibaSat_8x16.otb")) {
    // if(!SDCARD::open("/fonts/IBM_EGA_8x8.otb")) {
        con.printLn("Failed to open font");
        return false;
    }

    File* f = SDCARD::getFile();

    OTBFont font;

    if(!font.load(*f)){
        con.setColor(COLOR_RED);
        con.printLn("Font load error");
        con.useDefaultColor();
        return false;
    }

    char dump[1024];

    char16_t code = u'Я';
    const char* str = "Я";

    if(!font.debugPrintGlyph(value, dump, sizeof(dump))) {
        con.printLn("debugPrintGlyph error");     
        return false;
    }

    con.print(dump);

    SDCARD::close();

    return true;
}

static bool cmd_fs(Shell& shell) {
    auto& con = shell.console();
    auto& cmd = shell.parsedCmd();

    if (cmd.argc() < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: FS <path>");
        con.useDefaultColor();
        return false;
    }

    const char* path  = cmd.argv(1);


    if(!LittleFS.begin()) {
        con.print("LittleFS mount failed");
        return false;
    }

    File root = LittleFS.open(path);
    if(!root) {
        con.print("Failed to open directory ");
        con.printLn(path);
        return false;
    }

    if(!root.isDirectory()) {
        con.printLn("Not a directory");
        return false;
    }

    File file = root.openNextFile();

    while(file) {
        con.print(file.isDirectory() ? "[DIR]  " : "[FILE] ");
        con.print(file.name());
        con.print("  ");

        if(!file.isDirectory()) {
            con.printInt((int)file.size());
            con.print(" bytes");
        }

        con.printLn();

        file = root.openNextFile();
    }

    file.close();
    LittleFS.end();

    return true;
}

static bool cmd_view(Shell& shell) {
    auto& con = shell.console();
    auto& cmd = shell.parsedCmd();

    if (cmd.argc() < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: VIEW <path>");
        con.useDefaultColor();
        return false;
    }

    char path[MAX_PATH];
    shell.resolvePath(cmd.argv(1), path);

    if (!sdCheck(shell)) {
        return false;
    }

    if (!SDCARD::fileExists(path)) {
        con.setColor(COLOR_RED);
        con.print("File not found: ");
        con.printLn(path);
        con.useDefaultColor();
        return false;
    }

    shell.app().requestSwitch(
        new Viewer(path)
    ); 

    return true;
}
