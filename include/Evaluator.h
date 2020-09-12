#pragma once

#include "Processor.h"

class Evaluator : public Processor {
    // TODO!
    NodeVal performLoad(CodeLoc codeLoc, NamePool::Id id, const NodeVal &val) { msgs->errorInternal(codeLoc); return NodeVal(); }
    NodeVal performRegister(CodeLoc codeLoc, NamePool::Id id, TypeTable::Id ty) { msgs->errorInternal(codeLoc); return NodeVal(); }
    NodeVal performRegister(CodeLoc codeLoc, NamePool::Id id, const NodeVal &init) { msgs->errorInternal(codeLoc); return NodeVal(); }
    NodeVal performCast(CodeLoc codeLoc, const NodeVal &node, TypeTable::Id ty);
    bool performBlockSetUp(CodeLoc codeLoc, SymbolTable::Block &block) { msgs->errorInternal(codeLoc); return false; }
    void performBlockTearDown(CodeLoc codeLoc, const SymbolTable::Block &block, bool success) { msgs->errorInternal(codeLoc); }
    NodeVal performCall(CodeLoc codeLoc, const FuncValue &func, const std::vector<NodeVal> &args) { msgs->errorInternal(codeLoc); return NodeVal(); }
    bool performFunctionDeclaration(CodeLoc codeLoc, FuncValue &func) { msgs->errorInternal(codeLoc); return false; }
    bool performFunctionDefinition(const NodeVal &args, const NodeVal &body, FuncValue &func) { msgs->errorInternal(body.getCodeLoc()); return false; }
    bool performRet(CodeLoc codeLoc) { msgs->errorInternal(codeLoc); return false; }
    bool performRet(CodeLoc codeLoc, const NodeVal &node) { msgs->errorInternal(codeLoc); return false; }
    NodeVal performEvaluation(const NodeVal &node);
    NodeVal performOperUnary(CodeLoc codeLoc, const NodeVal &oper, Oper op) { msgs->errorInternal(codeLoc); return NodeVal(); }
    NodeVal performOperUnaryDeref(CodeLoc codeLoc, const NodeVal &oper) { msgs->errorInternal(codeLoc); return NodeVal(); }
    void* performOperComparisonSetUp(CodeLoc codeLoc, std::size_t opersCnt) { return nullptr; }
    std::optional<bool> performOperComparison(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op, void *signal) { return std::nullopt; }
    NodeVal performOperComparisonTearDown(CodeLoc codeLoc, bool success, void *signal) { msgs->errorInternal(codeLoc); return NodeVal(); }
    NodeVal performOperAssignment(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs) { msgs->errorInternal(codeLoc); return NodeVal(); }
    NodeVal performOperIndex(CodeLoc codeLoc, const NodeVal &base, const NodeVal &ind, TypeTable::Id resTy) { msgs->errorInternal(codeLoc); return NodeVal(); }
    NodeVal performOperMember(CodeLoc codeLoc, const NodeVal &base, std::uint64_t ind, TypeTable::Id resTy) { msgs->errorInternal(codeLoc); return NodeVal(); }
    NodeVal performOperRegular(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op) { msgs->errorInternal(codeLoc); return NodeVal(); }
    NodeVal performTuple(CodeLoc codeLoc, TypeTable::Id ty, const std::vector<NodeVal> &membs) { msgs->errorInternal(codeLoc); return NodeVal(); }

public:
    Evaluator(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompileMessages *msgs);
};