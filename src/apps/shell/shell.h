#pragma once
#include "ISubsystem.h"
#include "keyboard.h"
#include "console.h"
#include "VGA/VGA.h"
#include "sdcard.h"
#include "shell_lua.h"
#include "CmdParser.h"
#include "IShellCommand.h"

#define SHELL_CMD_MAX     64
#define MAX_SEGMENTS      16
#define MAX_PATH          128
#define PROMPT            "> "
#define PROMPT_LEN        2

#define HISTORY_SIZE      10
#define HISTORY_CMD_MAX   SHELL_CMD_MAX

class AppManager;

class Shell : public ISubsystem {
    public:
        Shell(AppManager& app);
        ~Shell();

        bool init() override;
        void update(float dt) override;
        void tick() override;

        CmdParser& parsedCmd() { return _cmdParser; }
        Console& console() { return _console; }
        ShellLua& lua() { return _lua; }
        AppManager& app() { return _app; }
        
        const char* cwd() const { return _cwd; }
        void setCwd(const char* path);
        void resolvePath(const char* input, char* out);
        void commandCancelRequest();   // Ctrl+C
        void setActiveCommand(IShellCommand* cmd);
        bool hasActiveCommand() const;

    private:
        AppManager& _app;

        /* ==== DEVICES ==== */
        Console _console;
        ShellLua _lua;
        CmdParser _cmdParser;

        bool _isEngLayout;

        IShellCommand* _activeCommand = nullptr;
        bool _commandCancelRequested = false;

        /* ==== COMMAND LINE ==== */
        char _cmd[SHELL_CMD_MAX];
        // uint16_t _cmd[SHELL_CMD_MAX]; // todo надо сделать для юникода то ломаются функции работы со строками
        int  _len;
        int  _cursorPos;

        /* ==== CURSOR ==== */
        int _cursorX;
        int _cursorY;

        /* ==== PATH / PROMPT ==== */
        char _cwd[128];

        /* ==== HISTORY ==== */
        char _history[HISTORY_SIZE][HISTORY_CMD_MAX];
        int  _historyCount;
        int  _historyHead;
        int  _historyPos;

        /* ==== PROMPT ==== */
        void printPrompt();
        void redrawInputLine();

        /* ==== INPUT HANDLERS ==== */
        void onChar(uint16_t c);
        void onKeyBack();
        void onKeyEnter();
        void onKeyLeft();
        void onKeyRight();
        void onKeyUp();
        void onKeyDown();
        void onPrintScreen();
        bool loadLogo(uint8_t*& img, int& w, int& h);

        /* ==== HISTORY ==== */
        void historyAdd(const char* line);
        void loadHistoryLine(const char* line);        
};