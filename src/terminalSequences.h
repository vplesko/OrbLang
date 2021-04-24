#pragma once

#include <string>

enum class TerminalColor {
    C_BLACK,
    C_RED,
    C_GREEN,
    C_YELLOW,
    C_BLUE,
    C_MAGENTA,
    C_CYAN,
    C_WHITE,
    C_NO_CHANGE
};

std::string terminalSet(TerminalColor col, bool bold);
std::string terminalSetBold();
std::string terminalReset();
