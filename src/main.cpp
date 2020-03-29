#include <iostream>
#include <filesystem>
#include "Compiler.h"
using namespace std;

const int BAD_ARGS = -1;
const int MULTI_OUT = -2;
const int PARSE_FAIL = -3;
const int COMPILE_FAIL = -4;

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

    cout << "Files:";
    for (const std::string &f : inputs)
        cout << " " << f;
    cout << endl;

    Compiler compiler;
    if (!compiler.parse(inputs)) {
        compiler.dumpMsgs(cerr);
        return PARSE_FAIL;
    }
    
    /*cout << "Code printout:" << endl;
    compiler.printout();*/

    if (!output.empty()) {
        // TODO link object files
        // TODO pass flags to clang
        string ext = filesystem::path(output).extension().string();
        bool obj = ext == ".o" || ext == ".obj";
        if (!compiler.compile(output, !obj)) {
            cerr << "Something went wrong when compiling!" << endl;
            return COMPILE_FAIL;
        }
    }

    return 0;
}