#pragma once

#include <string>
#include <vector>
#include <optional>

struct ProgramArgs {
    std::vector<std::string> inputs;
    std::string output;
    bool obj;

    static std::optional<ProgramArgs> parseArgs(int argc,  char** argv);
};