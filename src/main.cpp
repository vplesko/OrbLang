#include "SymbolTable.h"
#include "Lexer.h"
#include "Parser.h"
#include <fstream>
using namespace std;

int main(int argc,  char** argv) {
    if (argc < 2) {
        cout << "Specify input file name" << endl;
        return -1;
    }

    cout << "Parsing file: " << argv[1] << endl << endl;

    ifstream file(argv[1]);

    if (!file.is_open()) {
        cout << "Could not open file" << endl;
        return -2;
    }
    
    NamePool names;
    TypeTable types;
    SymbolTable symbols(&types);
    Lexer lex(&names);
    Parser par(&names, &symbols, &lex);
    par.parse(file);

    return 0;
}