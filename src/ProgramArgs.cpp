#include "ProgramArgs.h"
#include <iostream>
#include <filesystem>
#include "utils.h"
using namespace std;

optional<ProgramArgs> ProgramArgs::parseArgs(int argc,  char** argv, std::ostream &out) {
    ProgramArgs programArgs;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];

        if (arg.rfind("-O", 0) == 0) {
            char *end = nullptr;
            optional<unsigned> num;
            if (arg.size() > 2) num = strtoll(arg.c_str()+2, &end, 10);
            if (!num.has_value() || num > 3 || end != &*arg.end() || errno == ERANGE) {
                out << "Bad optimization level specified." << endl;
                return nullopt;
            }

            if (programArgs.optLvl.has_value()) {
                out << "Multiple optimization levels specified." << endl;
                return nullopt;
            }

            programArgs.optLvl = num;
        } else if (arg == "-o") {
            if (i+1 == argc) {
                out << "Argument to -o must be specified." << endl;
                return nullopt;
            }

            programArgs.output = argv[++i];
        } else if (arg == "-c") {
            programArgs.exe = false;
        } else {
            if (filesystem::path(arg).extension().string() == ".orb") {
                programArgs.inputsSrc.push_back(arg);
            } else {
                if (!filesystem::exists(arg)) {
                    out << "Nonexistent file '" << arg << "'." << endl;
                    return nullopt;
                }

                programArgs.inputsOther.push_back(arg);
            }
        }
    }

    if (programArgs.inputsSrc.empty() && programArgs.inputsOther.empty()) {
        out << "No input files specified." << endl;
        return nullopt;
    }

    if (programArgs.inputsSrc.empty() && !programArgs.exe) {
        out << "No source input files specified when linking not requested." << endl;
        return nullopt;
    }

    if (programArgs.output.empty()) {
        string firstInputStem;
        if (!programArgs.inputsSrc.empty()) firstInputStem = filesystem::path(programArgs.inputsSrc.front()).stem().string();
        else firstInputStem = filesystem::path(programArgs.inputsOther.front()).stem().string();

        if (programArgs.exe) programArgs.output = firstInputStem + (isOsWindows ? ".exe" : "");
        else programArgs.output = firstInputStem + (isOsWindows ? ".obj" : ".o");
    }

    return programArgs;
}