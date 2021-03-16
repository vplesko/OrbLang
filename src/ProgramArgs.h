#pragma once

#include <string>
#include <vector>
#include <optional>

// TODO! update docs and examples
// TODO! help text
struct ProgramArgs {
    std::vector<std::string> inputsSrc, inputsOther;
    std::string outputBin;
    std::optional<std::string> outputLlvm;
    bool link = true;
    std::optional<unsigned> optLvl;

    static std::optional<ProgramArgs> parseArgs(int argc,  char** argv, std::ostream &out);
};