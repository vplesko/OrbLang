#pragma once

#include <string>
#include <vector>
#include <optional>

// TODO! update docs and examples
// TODO! help text
struct ProgramArgs {
    std::vector<std::string> inputs;
    std::string output;
    bool exe = true;

    static std::optional<ProgramArgs> parseArgs(int argc,  char** argv);
};