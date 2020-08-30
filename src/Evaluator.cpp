#include "Evaluator.h"
using namespace std;

Evaluator::Evaluator(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompileMessages *msgs)
    : Processor(namePool, stringPool, typeTable, symbolTable, msgs) {
}

NodeVal Evaluator::evaluateNode(const NodeVal &node) {
    return processNode(node);
}