#include "Evaluator.h"
using namespace std;

Evaluator::Evaluator(NamePool *namePool, StringPool *stringPool, SymbolTable *symbolTable, CompileMessages *msgs, AstStorage *astStorage)
    : CodeProcessor(namePool, stringPool, symbolTable, msgs),
    astStorage(astStorage) {
    resetGotoIssuing();
}