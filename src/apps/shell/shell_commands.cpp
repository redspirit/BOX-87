#include "shell_commands.h"
#include "palette.h"
#include "shell.h"
#include "apps/helloworld/helloworld.h"
// #include "player/player.h"
#include "AppManager.h"
#include "sdcard.h"

#include <string.h>
#include <stdio.h>
#include <esp_system.h>

#define TYPE_MAX_SIZE 1024
#define SDSPEED_BUFFER  (32 * 1024)  // 32 KB

/* =========================================================
 *  COMMAND HANDLERS
 * ========================================================= */

static bool cmd_cls(Shell& shell, ShellParser& cmd);
static bool cmd_help(Shell& shell, ShellParser& cmd);
static bool cmd_reboot(Shell& shell, ShellParser& cmd);
static bool cmd_font(Shell& shell, ShellParser& cmd);
static bool cmd_palette(Shell& shell, ShellParser& cmd);
static bool cmd_pwd(Shell& shell, ShellParser& cmd);
static bool cmd_cd(Shell& shell, ShellParser& cmd);
static bool cmd_dir(Shell& shell, ShellParser& cmd);
static bool cmd_type(Shell& shell, ShellParser& cmd);
static bool cmd_mkdir(Shell& shell, ShellParser& cmd);
static bool cmd_rd(Shell& shell, ShellParser& cmd);
static bool cmd_del(Shell& shell, ShellParser& cmd);
static bool cmd_write(Shell& shell, ShellParser& cmd);
static bool cmd_append(Shell& shell, ShellParser& cmd);
static bool cmd_lua(Shell& shell, ShellParser& cmd);
static bool cmd_mem(Shell& shell, ShellParser& cmd);
static bool cmd_hw(Shell& shell, ShellParser& cmd);
static bool cmd_play(Shell& shell, ShellParser& cmd);
static bool cmd_pcm(Shell& shell, ShellParser& cmd);
static bool cmd_color(Shell& shell, ShellParser& cmd);
static bool cmd_sdspeed(Shell& shell, ShellParser& cmd);

/* =========================================================
 *  COMMAND TABLE
 * ========================================================= */

struct ShellCommand {
    const char* name;
    bool (*handler)(Shell&, ShellParser&);
    const char* help;
};

static const ShellCommand commands[] = {
    { "HELP",  cmd_help,  "Get this help" },
    { "CLS",  cmd_cls,  "Clear screen" },
    { "REBOOT", cmd_reboot, "Restart system" },
    { "FONT",   cmd_font,  "Show font table" },
    { "PALETTE",  cmd_palette,  "Show color palette" },
    { "PWD",  cmd_pwd,  "Show current directory" },
    { "CD",  cmd_cd,  "Change current directory" },
    { "DIR",  cmd_dir,  "List directory contents" },
    { "TYPE",  cmd_type,  "Display text file" },
    { "RD",    cmd_rd,    "Remove empty directory" },
    { "DEL",   cmd_del,   "Delete file" },
    { "MKDIR", cmd_mkdir, "Create directory" },
    { "WRITE",  cmd_write,  "Write text file or create empty" },
    { "APPEND", cmd_append, "Append text to file" },
    { "LUA", cmd_lua, "Execute Lua expression" },
    { "MEM", cmd_mem, "Show memory info" },
    { "HW", cmd_hw, "Start Hello World application" },
    { "PLAY", cmd_play, "Play media file" },
    { "PCM", cmd_pcm, "Play pcm audio file" },
    { "COLOR", cmd_color, "Show specific color" },
    { "SDSPEED", cmd_sdspeed, "Test speed of file read from sd" },
};

static const int commandCount =
    sizeof(commands) / sizeof(commands[0]);

static char hexDigit(uint8_t v) {
    return (v < 10) ? ('0' + v) : ('A' + (v - 10));
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

static const char* getTextArg(const ShellParser& cmd) {
    if (cmd.argc < 3)
        return nullptr;

    return cmd.argv[2];
}

/* =========================================================
 *  EXECUTOR
 * ========================================================= */

bool shellExecute(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();    
    if (cmd.argc == 0)
        return false;

    for (int i = 0; i < commandCount; ++i) {
        if (strcasecmp(cmd.argv[0], commands[i].name) == 0) {
            return commands[i].handler(shell, cmd);
        }
    }

    con.setColor(COLOR_RED);
    con.print("Unknown command: ");
    con.print(cmd.argv[0]);
    con.useDefaultColor();
    con.printLn();

    return false;
}

/* =========================================================
 *  COMMAND IMPLEMENTATIONS
 * ========================================================= */

static bool cmd_help(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();  

    if (cmd.argc == 2) {
        for (int i = 0; i < commandCount; ++i) {
            if (strcasecmp(cmd.argv[1], commands[i].name) == 0) {

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
        con.printLn(cmd.argv[1]);
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
            con.print(' ');

        con.printLn(commands[i].help);
    }

    return true;
}

static bool cmd_cls(Shell& shell, ShellParser& cmd) {
    shell.console().clear();
    return true;
}

static bool cmd_reboot(Shell& shell, ShellParser& cmd) {
    shell.console().printLn("Rebooting...");
    esp_restart();
    return true;
}

static bool cmd_font(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();

    con.setColor(COLOR_CYAN);
    con.printLn("FONT TABLE (0x00-0xFF)");
    con.printRawChar((char)205, 22);
    con.printLn();

    for (int base = 0; base < 256; base += 16) {
        con.setColor(COLOR_YELLOW);
        con.print("0x");
        con.print(hexDigit((base >> 4) & 0xF));
        con.print(hexDigit(base & 0xF));
        con.print(" ");

        // 16 символов
        con.setColor(COLOR_WHITE);
        for (int i = 0; i < 16; i++) {
            con.printRawChar((char)(base + i));
        }
        con.printLn();
    }
    con.useDefaultColor();
    return true;
}

static bool cmd_palette(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();

    con.setColor(COLOR_CYAN);
    con.printLn("COLOR PALETTE (0x00-0xFF)");
    con.printRawChar((char)205, 24);
    con.printLn();

    for (int base = 0; base < 256; base += 16) {
        // Заголовок строки: 0xNN
        con.setColor(COLOR_YELLOW);
        con.print("0x");
        con.print(hexDigit((base >> 4) & 0xF));
        con.print(hexDigit(base & 0xF));
        con.print(" ");

        // 16 цветов
        for (int i = 0; i < 16; i++) {
            uint8_t color = base + i;
            con.setColor(color);
            con.print((char)219); // █
        }

        con.printLn();
    }
    con.useDefaultColor();
    return true;
}

static bool cmd_pwd(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();
    con.setColor(COLOR_YELLOW);
    con.printLn(shell.cwd());
    con.useDefaultColor();
    return true;
}

static bool cmd_cd(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();

    if (cmd.argc < 2) {
        shell.setCwd("/");
        return true;
    }

    char newPath[MAX_PATH];
    shell.resolvePath(cmd.argv[1], newPath);

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

static bool cmd_dir(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();

    char path[MAX_PATH];

    // DIR или DIR <path>
    if (cmd.argc > 1) {
        shell.resolvePath(cmd.argv[1], path);
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

static bool cmd_type(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();

    if (cmd.argc < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: TYPE <file>");
        con.useDefaultColor();
        return false;
    }

    char path[MAX_PATH];
    shell.resolvePath(cmd.argv[1], path);

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

static bool cmd_mkdir(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();

    if (cmd.argc < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: MKDIR <dir>");
        con.useDefaultColor();
        return false;
    }

    char path[MAX_PATH];
    shell.resolvePath(cmd.argv[1], path);

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

static bool cmd_rd(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();

    if (cmd.argc < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: RD <dir>");
        con.useDefaultColor();
        return false;
    }

    char path[MAX_PATH];
    shell.resolvePath(cmd.argv[1], path);

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

static bool cmd_del(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();

    if (cmd.argc < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: DEL <file>");
        con.useDefaultColor();
        return false;
    }

    char path[MAX_PATH];
    shell.resolvePath(cmd.argv[1], path);

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

static bool cmd_write(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();

    if (cmd.argc < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: WRITE <file> [text]");
        con.useDefaultColor();
        return false;
    }

    char path[MAX_PATH];
    shell.resolvePath(cmd.argv[1], path);

    const char* text = getTextArg(cmd);

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

static bool cmd_append(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();

    if (cmd.argc < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: APPEND <file> <text>");
        con.useDefaultColor();
        return false;
    }

    char path[MAX_PATH];
    shell.resolvePath(cmd.argv[1], path);

    const char* text = getTextArg(cmd);

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

static bool cmd_sdspeed(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();

    if (cmd.argc < 1) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: SDSPEED <file>");
        con.useDefaultColor();
        return false;
    }

    char path[MAX_PATH];
    shell.resolvePath(cmd.argv[1], path);

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

            con.print((int)percent); con.print("%  ");
            con.print((int)totalRead); con.print(" bytes ");
            con.print((int)speedKB); con.printLn(" KB/s");

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
    con.print("File size: "); con.printLn(fileSize);
    con.print("Time: "); con.print((int)totalTime); con.printLn(" ms");
    con.print("Average speed: "); con.print((int)avgKB); con.printLn(" KB/s");    

    return true;
}

static bool cmd_mem(Shell& shell, ShellParser& cmd) {
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
    con.print((int)(ram_total >> 10));
    con.printLn(" KB");

    con.print("  FREE : ");
    con.print((int)(ram_free >> 10));
    con.printLn(" KB");

    con.print("  USED : ");
    con.print((int)((ram_total - ram_free) >> 10));
    con.printLn(" KB");

    con.print("  MIN  : ");
    con.print((int)(ram_min >> 10));
    con.printLn(" KB");

    if (psram_total > 0) {
        con.printLn("");
        con.printLn("PSRAM");

        con.print("  TOTAL: ");
        con.print((int)(psram_total >> 10));
        con.printLn(" KB");

        con.print("  FREE : ");
        con.print((int)(psram_free >> 10));
        con.printLn(" KB");

        con.print("  USED : ");
        con.print((int)((psram_total - psram_free) >> 10));
        con.printLn(" KB");

        con.print("  MIN  : ");
        con.print((int)(psram_min >> 10));
        con.printLn(" KB");
    } else {
        con.printLn("");
        con.printLn("PSRAM: NOT PRESENT");
    }    

    return true;
}

static bool cmd_lua(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();

    if (cmd.argc < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: LUA <expression>");
        con.useDefaultColor();
        return false;
    }

    // всё после LUA — выражение
    const char* expr = cmd.argv[1];

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

static bool cmd_hw(Shell& shell, ShellParser& cmd) {
    shell.app().requestSwitch(
        new HelloWorld()
    );
    return true;
}

static bool cmd_play(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();

    if (cmd.argc < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: PLAY <file>");
        con.useDefaultColor();
        return false;
    }

    char path[MAX_PATH];
    shell.resolvePath(cmd.argv[1], path);

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

static bool cmd_pcm(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();

    if (cmd.argc < 2) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: PCM <file>");
        con.useDefaultColor();
        return false;
    }

    char path[MAX_PATH];
    shell.resolvePath(cmd.argv[1], path);

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

static bool cmd_color(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();

    if (cmd.argc < 3) {
        con.setColor(COLOR_RED);
        con.printLn("Usage: COLOR BIN 11111111");
        con.printLn("Usage: COLOR HEX FF");
        con.printLn("Usage: COLOR RGB <R> <G> <B>");      
        con.printLn("Usage: COLOR GRADIENT BLUE");      
        con.useDefaultColor();
        return false;
    }
    const char* mode  = cmd.argv[1];
    const char* value = cmd.argv[2];

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
        if (cmd.argc < 5) {
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
        if (!parseByte(cmd.argv[2], r) ||
            !parseByte(cmd.argv[3], g) ||
            !parseByte(cmd.argv[4], b)) {
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
            con.printRawChar((char)219, 20);
            con.useDefaultColor();
            con.print(cv);
            con.printLn();
        }        
        
        // green gradient
        for (int cv = 0; cv < 8; cv++) {
            con.setColorRaw(cv << 3);
            con.printRawChar((char)219, 20);
            con.useDefaultColor();
            con.print(cv);
            con.printLn();
        }

        // red gradient
        for (int cv = 0; cv < 8; cv++) {
            con.setColorRaw(cv);
            con.printRawChar((char)219, 20);
            con.useDefaultColor();
            con.print(cv);
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