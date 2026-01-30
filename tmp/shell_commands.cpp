#include "shell_commands.h"
#include "palette.h"

#include <string.h>
#include <stdio.h>

/* =========================================================
 *  COMMAND HANDLERS
 * ========================================================= */

static bool cmd_help(ParsedCommand& cmd);
static bool cmd_clear(ParsedCommand& cmd);
static bool cmd_echo(ParsedCommand& cmd);
static bool cmd_version(ParsedCommand& cmd);

/* =========================================================
 *  COMMAND TABLE
 * ========================================================= */

struct ShellCommand {
    const char* name;
    bool (*handler)(Shell&, ParsedCommand&);
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

bool shellExecute(Shell& shell, ParsedCommand& cmd) {
    if (cmd.argc == 0)
        return false;

    for (int i = 0; i < g_commandCount; ++i) {
        if (strcmp(cmd.argv[0], g_commands[i].name) == 0) {
            return g_commands[i].handler(shell, cmd);
        }
    }

    shell.console().setColor(COLOR_RED);
    shell.console().print("Unknown command: ");
    shell.console().print(cmd.argv[0]);
    shell.console().useDefaultColor();
    shell.console().printLn();

    return false;
}

/* =========================================================
 *  COMMAND IMPLEMENTATIONS
 * ========================================================= */

static bool cmd_help(Shell& shell, ParsedCommand& cmd) {
    g_console->printLn("Available commands:");

    for (int i = 0; i < g_commandCount; ++i) {
        g_console->print("  ");
        g_console->print(g_commands[i].name);
        g_console->print(" - ");
        g_console->printLn(g_commands[i].help);
    }

    return true;
}

static bool cmd_clear(Shell& shell, ParsedCommand& cmd) {
    g_console->clear();
    return true;
}

static bool cmd_echo(Shell& shell, ParsedCommand& cmd) {
    for (int i = 1; i < cmd.argc; ++i) {
        g_console->print(cmd.argv[i]);
        if (i != cmd.argc - 1)
            g_console->print(" ");
    }
    g_console->printLn();
    return true;
}

static bool cmd_version(Shell& shell, ParsedCommand& cmd) {
    g_console->printLn("RetroShell v0.1");
    g_console->printLn("Target: ESP32 / VGA");
    return true;
}
