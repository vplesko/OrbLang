#include <filesystem>
#include <iostream>
#include "CompilationOrchestrator.h"
#include "exceptions.h"
#include "ProgramArgs.h"
using namespace std;

enum ProgramError {
    BAD_ARGS = 1,
    PROCESS_FAIL,
    COMPILE_FAIL,
    // codes >= 100 indicate internal errors
    // if changing, update Python test script
    INTERNAL = 100
};

int main(int argc,  char** argv) {
    optional<ProgramArgs> programArgs = ProgramArgs::parseArgs(argc, argv, cerr);
    if (!programArgs.has_value()) {
        ProgramArgs::printHelp(cout);
        return BAD_ARGS;
    }

    CompilationOrchestrator co(move(programArgs.value()), cerr);

    try {
        if (!co.process()) {
            cerr << "Processing failed." << endl;
            return co.isInternalError() ? INTERNAL : PROCESS_FAIL;
        }

        if (!co.compile()) {
            cerr << "Compilation failed." << endl;
            return co.isInternalError() ? INTERNAL : COMPILE_FAIL;
        }

        co.printout();
    } catch (ExceptionEvaluatorJump ex) {
        cerr << "Something went wrong when compiling!" << endl;
        return INTERNAL;
    }

    return 0;
}