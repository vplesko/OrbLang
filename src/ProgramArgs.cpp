#include "ProgramArgs.h"
#include <iostream>
#include <filesystem>
#include "utils.h"
using namespace std;

optional<ProgramArgs> ProgramArgs::parseArgs(int argc,  char** argv) {
    ProgramArgs programArgs;

    for (int i = 1; i < argc; ++i) {
        if (argv[i] == string("-o")) {
            if (i+1 == argc) {
                cerr << "Argument to -o must be specified." << endl;
                return nullopt;
            }

            ++i;
            programArgs.output = argv[i];
        } else if (argv[i] == string("-c")) {
            programArgs.exe = false;
        } else {
            if (filesystem::path(argv[i]).extension().string() == ".orb") {
                programArgs.inputsSrc.push_back(argv[i]);
            } else {
                if (!filesystem::exists(argv[i])) {
                    cerr << "Nonexistent file '" << argv[i] << "'." << endl;
                    return nullopt;
                }

                programArgs.inputsOther.push_back(argv[i]);
            }
        }
    }

    if (programArgs.inputsSrc.empty() && programArgs.inputsOther.empty()) {
        cerr << "No input files specified." << endl;
        return nullopt;
    }

    if (programArgs.inputsSrc.empty() && !programArgs.exe) {
        cerr << "No source input files specified when linking not requested." << endl;
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