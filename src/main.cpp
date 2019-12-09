#include "Compiler.h"
#include <iostream>
using namespace std;

int main(int argc,  char** argv) {
    if (argc < 2) {
        cout << "Specify input file name" << endl;
        return -1;
    }

    cout << "File: " << argv[1] << endl;

    Compiler compiler;
    if (!compiler.parse(argv[1])) {
        cout << "Something went wrong when parsing!" << endl;
        return -2;
    }
    
    /*cout << "Code printout:" << endl;
    compiler.printout();*/

    if (argc >= 3) {
        if (!compiler.compile(argv[2])) {
            cout << "Something went wrong when compiling!" << endl;
            return -3;
        }
    }

    return 0;
}