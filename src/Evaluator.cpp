#include "Evaluator.h"
using namespace std;

Evaluator::Evaluator(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompileMessages *msgs)
    : Processor(namePool, stringPool, typeTable, symbolTable, msgs) {
}

NodeVal Evaluator::performCast(CodeLoc codeLoc, const NodeVal &node, TypeTable::Id ty) {
    // TODO!
    // TODO don't forget str to char arr
    msgs->errorInternal(node.getCodeLoc());
    return NodeVal();
}

NodeVal Evaluator::performEvaluation(const NodeVal &node) {
    return processNode(node);
}