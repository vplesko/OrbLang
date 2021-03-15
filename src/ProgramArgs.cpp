#include "ProgramArgs.h"
#include <iostream>
#include <filesystem>
#include "utils.h"
using namespace std;

optional<ProgramArgs> ProgramArgs::parseArgs(int argc,  char** argv) {
    if (argc < 2) {
        cerr << "Not enough arguments when calling orbc." << endl;
        return nullopt;
    }

    ProgramArgs programArgs;

    for (int i = 1; i < argc; ++i) {
        string in(argv[i]);
        if (filesystem::path(in).extension().string() == ".orb") {
            programArgs.inputs.push_back(in);
        } else {
            if (!programArgs.output.empty()) {
                cerr << "Cannot have multiple outputs." << endl;
                return nullopt;
            } else {
                programArgs.output = in;
            }
        }
    }

    if (programArgs.inputs.empty()) {
        cerr << "No input files specified." << endl;
        return nullopt;
    }

    if (programArgs.output.empty()) {
        programArgs.output = isOsWindows ? "a.obj" : "a.o";
    }

    string ext = filesystem::path(programArgs.output).extension().string();
    programArgs.obj = ext == ".o" || ext == ".obj";

    cout << "File(s):";
    for (const std::string &f : programArgs.inputs)
        cout << " " << f;
    cout << endl;

    return programArgs;
}