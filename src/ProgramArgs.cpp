#include "ProgramArgs.h"
#include <iostream>
#include <filesystem>
#include "utils.h"
using namespace std;

optional<ProgramArgs> ProgramArgs::parseArgs(int argc,  char** argv, std::ostream &out) {
    ProgramArgs programArgs;

    bool emitLlvm = false;

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

            programArgs.outputBin = argv[++i];
        } else if (arg == "-c") {
            programArgs.link = false;
        } else if (arg == "-emit-llvm") {
            emitLlvm = true;
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

    if (programArgs.inputsSrc.empty()) {
        bool failure = false;

        if (programArgs.inputsOther.empty()) {
            out << "No input files specified." << endl;
            failure = true;
        }
        if (!programArgs.link) {
            out << "No source input files specified when linking disabled." << endl;
            failure = true;
        }
        if (emitLlvm) {
            out << "No source input files specified when emitting LLVM output requested." << endl;
            failure = true;
        }

        if (failure) return nullopt;
    }

    if (!programArgs.link && !programArgs.inputsOther.empty()) {
        out << "Only source input files may be specified when linking disabled." << endl;
        return nullopt;
    }

    string firstInputStem;
    if (!programArgs.inputsSrc.empty()) firstInputStem = filesystem::path(programArgs.inputsSrc.front()).stem().string();
    else firstInputStem = filesystem::path(programArgs.inputsOther.front()).stem().string();

    if (programArgs.outputBin.empty()) {
        if (programArgs.link) programArgs.outputBin = firstInputStem + (isOsWindows ? ".exe" : "");
        else programArgs.outputBin = firstInputStem + (isOsWindows ? ".obj" : ".o");
    }

    if (emitLlvm) {
        programArgs.outputLlvm = firstInputStem + ".ll";
    }

    return programArgs;
}

void ProgramArgs::printHelp(std::ostream &out) {
    out << R"orbc_help(
Usage: orbc [options] file...
Options:
  -c          Only process and compile, but do not link.
  -emit-llvm  Print the LLVM representation into a .ll file.
  -o <file>   Place the binary output into <file>.
  -O<number>  Set the optimization level. Valid values are -O0, -O1, -O2, and -O3.
)orbc_help";
}