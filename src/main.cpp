#include <iostream>
#include <filesystem>
#include "CompilationOrchestrator.h"
#include "exceptions.h"
#include "ProgramArgs.h"
using namespace std;

enum Error {
    BAD_ARGS = 1,
    PROCESS_FAIL,
    COMPILE_FAIL,
    // codes >= 100 indicate internal errors
    // if changing, update Python test script
    INTERNAL = 100
};

int main(int argc,  char** argv) {
    optional<ProgramArgs> programArgs = ProgramArgs::parseArgs(argc, argv);
    if (!programArgs.has_value()) return BAD_ARGS;

    try {
        CompilationOrchestrator co(cerr);
        if (!co.process(programArgs.value().inputs)) {
            cerr << "Processing failed." << endl;
            return co.isInternalError() ? INTERNAL : PROCESS_FAIL;
        }

        /*cout << "Code printout:" << endl;
        co.printout();*/

        if (!co.compile(programArgs.value())) {
            cerr << "Compilation failed." << endl;
            return co.isInternalError() ? INTERNAL : COMPILE_FAIL;
        }
    } catch (ExceptionEvaluatorJump ex) {
        cerr << "Something went wrong when compiling!" << endl;
        return INTERNAL;
    }

    return 0;
}