#pragma once

#include <string>
#include <vector>
#include <optional>

// TODO! update docs and examples
// TODO! help text
struct ProgramArgs {
    std::vector<std::string> inputsSrc, inputsOther;
    std::string output;
    bool exe = true;
    std::optional<unsigned> optLvl;

    static std::optional<ProgramArgs> parseArgs(int argc,  char** argv, std::ostream &out);
};