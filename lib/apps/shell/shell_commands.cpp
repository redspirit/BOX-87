#include "shell_commands.h"
#include "palette.h"
#include "shell.h"

#include <string.h>
#include <stdio.h>

/* =========================================================
 *  COMMAND HANDLERS
 * ========================================================= */

static bool cmd_help(Shell& shell, ShellParser& cmd);
static bool cmd_clear(Shell& shell, ShellParser& cmd);
static bool cmd_echo(Shell& shell, ShellParser& cmd);
static bool cmd_version(Shell& shell, ShellParser& cmd);

/* =========================================================
 *  COMMAND TABLE
 * ========================================================= */

struct ShellCommand {
    const char* name;
    bool (*handler)(Shell&, ShellParser&);
    const char* help;
};

static const ShellCommand g_commands[] = {
    { "help",    cmd_help,    "Show this help" },
    { "clear",   cmd_clear,   "Clear screen" },
    { "echo",    cmd_echo,    "Print arguments" },
    { "version", cmd_version, "Show system version" },
};

static const int g_commandCount =
    sizeof(g_commands) / sizeof(g_commands[0]);

/* =========================================================
 *  EXECUTOR
 * ========================================================= */

bool shellExecute(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();    
    if (cmd.argc == 0)
        return false;

    for (int i = 0; i < g_commandCount; ++i) {
        if (strcmp(cmd.argv[0], g_commands[i].name) == 0) {
            return g_commands[i].handler(shell, cmd);
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

    con.printLn("Available commands:");

    for (int i = 0; i < g_commandCount; ++i) {
        con.print("  ");
        con.print(g_commands[i].name);
        con.print(" - ");
        con.printLn(g_commands[i].help);
    }

    return true;
}

static bool cmd_clear(Shell& shell, ShellParser& cmd) {
    shell.console().clear();
    return true;
}

static bool cmd_echo(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();

    for (int i = 1; i < cmd.argc; ++i) {
        con.print(cmd.argv[i]);
        if (i != cmd.argc - 1)
            con.print(" ");
    }
    con.printLn();
    return true;
}

static bool cmd_version(Shell& shell, ShellParser& cmd) {
    auto& con = shell.console();

    con.printLn("RetroShell v0.1");
    con.printLn("Target: ESP32 / VGA");
    return true;
}
