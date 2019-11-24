#include "Compiler.h"
#include <iostream>
using namespace std;

int main(int argc,  char** argv) {
    if (argc < 2) {
        cout << "Specify input file name" << endl;
        return -1;
    }

    cout << "File: " << argv[1] << endl << endl;

    Compiler com;
    if (!com.compile(argv[1])) {
        cout << "Something went wrong!" << endl;
    }

    return 0;
}