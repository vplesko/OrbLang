#include "Lexer.h"
#include "Parser.h"
#include <fstream>
using namespace std;

int main(int argc,  char** argv) {
    if (argc < 2) {
        cout << "Specify input file name" << endl;
        return -1;
    }

    ifstream file(argv[1]);

    if (!file.is_open()) {
        cout << "Could not open file" << endl;
        return -2;
    }
    
    Lexer lex(file);
    Parser par(&lex);
    par.parse();

    return 0;
}