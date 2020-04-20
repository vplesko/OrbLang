#include <iostream>
#include <filesystem>
#include "Compiler.h"
using namespace std;

enum Error {
    BAD_ARGS = 1,
    MULTI_OUT,
    NO_IN,
    PARSE_FAIL,
    COMPILE_FAIL
};

// TODO print error if input file invalid
int main(int argc,  char** argv) {
    if (argc < 2) {
        cerr << "Not enough arguments when calling orbc." << endl;
        return BAD_ARGS;
    }

    vector<string> inputs;
    string output;

    for (int i = 1; i < argc; ++i) {
        string in(argv[i]);
        if (filesystem::path(in).extension().string() == ".orb") {
            inputs.push_back(in);
        } else {
            if (!output.empty()) {
                cerr << "Cannot have multiple outputs." << endl;
                return MULTI_OUT;
            } else {
                output = in;
            }
        }
    }

    if (inputs.empty()) {
        cerr << "No input files specified." << endl;
        return NO_IN;
    }

    cout << "File(s):";
    for (const std::string &f : inputs)
        cout << " " << f;
    cout << endl;

    Compiler compiler(cerr);
    if (!compiler.parse(inputs)) {
        cerr << "Compilation failed." << endl;
        return PARSE_FAIL;
    }
    
    /*cout << "Code printout:" << endl;
    compiler.printout();*/

    if (!output.empty()) {
        string ext = filesystem::path(output).extension().string();
        bool obj = ext == ".o" || ext == ".obj";
        if (!compiler.compile(output, !obj)) {
            cerr << "Something went wrong when compiling!" << endl;
            return COMPILE_FAIL;
        }
    }

    return 0;
}