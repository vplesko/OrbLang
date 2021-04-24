#include "terminalSequences.h"
#include <optional>
#include <sstream>
#include "OrbCompilerConfig.h"
using namespace std;

#if PLATFORM_WINDOWS
#include <windows.h>
#endif

bool enableVirtualTerminalProcessing() {
#if PLATFORM_WINDOWS && defined(ENABLE_VIRTUAL_TERMINAL_PROCESSING)
    static optional<bool> oldResult;

    if (!oldResult.has_value()) {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) {
            oldResult = false;
            return false;
        }

        DWORD dwMode;
        if (!GetConsoleMode(hOut, &dwMode)) {
            oldResult = false;
            return false;
        }

        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        if (!SetConsoleMode(hOut, dwMode)) {
            oldResult = false;
            return false;
        }

        oldResult = true;
    }

    return oldResult.value();
#else
    return PLATFORM_UNIX;
#endif
}

string terminalSet(TerminalColor col, bool bold) {
    if (!enableVirtualTerminalProcessing()) return "";

    if (col == TerminalColor::C_NO_CHANGE && !bold) return "";

    stringstream ss;

    ss << "\033[";

    bool colSet = true;
    switch (col) {
    case TerminalColor::C_BLACK:
        ss << "30";
        break;
    case TerminalColor::C_RED:
        ss << "31";
        break;
    case TerminalColor::C_GREEN:
        ss << "32";
        break;
    case TerminalColor::C_YELLOW:
        ss << "33";
        break;
    case TerminalColor::C_BLUE:
        ss << "34";
        break;
    case TerminalColor::C_MAGENTA:
        ss << "35";
        break;
    case TerminalColor::C_CYAN:
        ss << "36";
        break;
    case TerminalColor::C_WHITE:
        ss << "37";
        break;
    default:
        colSet = false;
        break;
    }

    if (bold) {
        if (colSet) ss << ";1";
        else ss << "1";
    }

    ss << "m";

    return ss.str();
}

string terminalSetBold() {
    return "\033[1m";
}

string terminalReset() {
    return "\033[0m";
}
