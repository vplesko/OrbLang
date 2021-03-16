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
            programArgs.inputs.push_back(argv[i]);
        }
    }

    if (programArgs.inputs.empty()) {
        cerr << "No input files specified." << endl;
        return nullopt;
    }

    if (programArgs.output.empty()) {
        string firstInputStem = filesystem::path(programArgs.inputs.front()).stem().string();
        if (programArgs.exe) programArgs.output = firstInputStem + (isOsWindows ? ".exe" : "");
        else programArgs.output = firstInputStem + (isOsWindows ? ".obj" : ".o");
    }

    return programArgs;
}