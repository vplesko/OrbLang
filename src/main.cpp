#include <iostream>
#include "Compiler.h"
using namespace std;

int main(int argc,  char** argv) {
    if (argc < 3) {
        cout << "Not enough arguments when calling orbc." << endl;
        return -1;
    }

    vector<string> inputs;
    string output;

    for (int i = 1; i + 1 < argc; ++i) {
        string in(argv[i]);
        inputs.push_back(in);
    }

    cout << "Files:";
    for (const std::string &f : inputs)
        cout << " " << f;
    cout << endl;

    Compiler compiler;
    if (!compiler.parse(inputs)) {
        cout << "Something went wrong when parsing!" << endl;
        return -2;
    }
    
    /*cout << "Code printout:" << endl;
    compiler.printout();*/

    if (!compiler.compile(argv[argc-1])) {
        cout << "Something went wrong when compiling!" << endl;
        return -3;
    }

    return 0;
}