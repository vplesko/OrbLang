#pragma once

#include <optional>
#include <string>
#include <vector>

struct ProgramArgs {
    std::vector<std::string> inputsSrc, inputsOther, importPaths;
    std::string outputBin;
    std::optional<std::string> outputLlvm;
    bool link = true;
    std::optional<unsigned> optLvl;

    static std::optional<ProgramArgs> parseArgs(int argc, char** argv, std::ostream &out);
    static void printHelp(std::ostream &out);
};