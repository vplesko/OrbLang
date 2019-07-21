#include "Lexer.h"
#include "Parser.h"
#include <fstream>
using namespace std;

int main() {
    ifstream file("test.txt");
    
    Lexer lex(file);
    Parser par(&lex);
    par.parse();

    return 0;
}