#pragma once

#include "Processor.h"

class Evaluator : public Processor {
    // TODO!
    NodeVal performLoad(CodeLoc codeLoc, NamePool::Id id) { msgs->errorInternal(codeLoc); return NodeVal(); }
    // TODO don't forget str to char arr
    NodeVal performCast(const NodeVal &node, TypeTable::Id ty) { msgs->errorInternal(node.getCodeLoc()); return NodeVal(); }
    NodeVal performCall(CodeLoc codeLoc, const FuncValue &func, const std::vector<NodeVal> &args) { msgs->errorInternal(codeLoc); return NodeVal(); }
    bool performFunctionDeclaration(FuncValue &func) { return false; }
    bool performFunctionDefinition(const NodeVal &args, const NodeVal &body, FuncValue &func) { return false; }
    NodeVal performEvaluation(const NodeVal &node);

public:
    Evaluator(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompileMessages *msgs);
};