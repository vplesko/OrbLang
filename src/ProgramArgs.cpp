#include "ProgramArgs.h"
#include <filesystem>
#include <iostream>
#include "OrbCompilerConfig.h"
using namespace std;

optional<ProgramArgs> ProgramArgs::parseArgs(int argc,  char** argv, std::ostream &out) {
    ProgramArgs programArgs;

    bool emitLlvm = false;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];

        if (arg == "-c") {
            programArgs.link = false;
        } else if (arg == "-emit-llvm") {
            emitLlvm = true;
        } else if (arg == "-o") {
            if (i+1 == argc) {
                out << "Argument to -o must be specified." << endl;
                return nullopt;
            }

            programArgs.outputBin = argv[++i];
        } else if (arg.rfind("-O", 0) == 0) {
            char *end = nullptr;
            unsigned long num;
            if (arg.size() > 2) num = strtoul(arg.c_str()+2, &end, 10);
            if (errno == ERANGE || end != &*arg.end() || num > 3) {
                out << "Bad optimization level specified." << endl;
                return nullopt;
            }

            if (programArgs.optLvl.has_value()) {
                out << "Multiple optimization levels specified." << endl;
                return nullopt;
            }

            programArgs.optLvl = static_cast<unsigned>(num);
        } else if (arg.rfind("-I", 0) == 0) {
            string importPath = arg.substr(2);
            if (importPath.empty()) {
                out << "Empty import path specified." << endl;
                return nullopt;
            }
            programArgs.importPaths.push_back(move(importPath));
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
        if (programArgs.link) programArgs.outputBin = firstInputStem + (PLATFORM_WINDOWS ? ".exe" : "");
        else programArgs.outputBin = firstInputStem + (PLATFORM_WINDOWS ? ".obj" : ".o");
    }

    if (emitLlvm) {
        programArgs.outputLlvm = firstInputStem + ".ll";
    }

    return programArgs;
}

void ProgramArgs::printHelp(std::ostream &out) {
    out << R"orbc_help(
Usage: orbc [options] file...

Files can be .orb or object files.

Options:
  -c         Only process and compile, but do not link.
  -emit-llvm Print the LLVM representation into a .ll file.
  -I<dir>    Add directory <dir> to import search paths.
  -o <file>  Place the binary output into <file>.
  -O<num>    Set the optimization level. -O0, -O1, -O2, and -O3 are valid.
)orbc_help";
}