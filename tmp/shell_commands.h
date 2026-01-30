#pragma once

#include "ParsedCommand.h"

class Shell;
bool shellExecute(Shell& shell, ParsedCommand& cmd);