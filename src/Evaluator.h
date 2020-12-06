#pragma once

#include "Processor.h"

class Evaluator : public Processor {
    friend class Processor;

    std::optional<NodeVal> retVal;

    bool assignBasedOnTypeI(EvalVal &val, std::int64_t x, TypeTable::Id ty);
    bool assignBasedOnTypeU(EvalVal &val, std::uint64_t x, TypeTable::Id ty);
    bool assignBasedOnTypeF(EvalVal &val, double x, TypeTable::Id ty);
    bool assignBasedOnTypeC(EvalVal &val, char x, TypeTable::Id ty);
    bool assignBasedOnTypeB(EvalVal &val, bool x, TypeTable::Id ty);

    std::optional<EvalVal> makeCast(const EvalVal &srcEvalVal, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId);
    std::optional<EvalVal> makeArray(TypeTable::Id arrTypeId);
    std::vector<NodeVal> makeRawConcat(const EvalVal &lhs, const EvalVal &rhs) const;

    NodeVal doBlockTearDown(CodeLoc codeLoc, const SymbolTable::Block &block, bool success, bool jumpingOut);

public:
    NodeVal performLoad(CodeLoc codeLoc, NamePool::Id id, NodeVal &ref) override;
    NodeVal performRegister(CodeLoc codeLoc, NamePool::Id id, TypeTable::Id ty) override;
    NodeVal performRegister(CodeLoc codeLoc, NamePool::Id id, const NodeVal &init) override;
    NodeVal performCast(CodeLoc codeLoc, const NodeVal &node, TypeTable::Id ty) override;
    bool performBlockSetUp(CodeLoc codeLoc, SymbolTable::Block &block) override { return true; }
    std::optional<bool> performBlockBody(CodeLoc codeLoc, const SymbolTable::Block &block, const NodeVal &nodeBody) override;
    NodeVal performBlockTearDown(CodeLoc codeLoc, const SymbolTable::Block &block, bool success) override;
    bool performExit(CodeLoc codeLoc, const SymbolTable::Block &block, const NodeVal &cond) override;
    bool performLoop(CodeLoc codeLoc, const SymbolTable::Block &block, const NodeVal &cond) override;
    bool performPass(CodeLoc codeLoc, SymbolTable::Block &block, const NodeVal &val) override;
    NodeVal performCall(CodeLoc codeLoc, const FuncValue &func, const std::vector<NodeVal> &args) override;
    NodeVal performInvoke(CodeLoc codeLoc, const MacroValue &macro, const std::vector<NodeVal> &args) override;
    bool performFunctionDeclaration(CodeLoc codeLoc, FuncValue &func) override;
    bool performFunctionDefinition(const NodeVal &args, const NodeVal &body, FuncValue &func) override;
    bool performMacroDefinition(const NodeVal &args, const NodeVal &body, MacroValue &macro) override;
    bool performRet(CodeLoc codeLoc) override;
    bool performRet(CodeLoc codeLoc, const NodeVal &node) override;
    NodeVal performOperUnary(CodeLoc codeLoc, const NodeVal &oper, Oper op) override;
    NodeVal performOperUnaryDeref(CodeLoc codeLoc, const NodeVal &oper) override;
    void* performOperComparisonSetUp(CodeLoc codeLoc, std::size_t opersCnt) override;
    std::optional<bool> performOperComparison(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op, void *signal) override;
    NodeVal performOperComparisonTearDown(CodeLoc codeLoc, bool success, void *signal) override;
    NodeVal performOperAssignment(CodeLoc codeLoc, NodeVal &lhs, const NodeVal &rhs) override;
    NodeVal performOperIndex(CodeLoc codeLoc, NodeVal &base, const NodeVal &ind, TypeTable::Id resTy) override;
    NodeVal performOperMember(CodeLoc codeLoc, NodeVal &base, std::uint64_t ind, TypeTable::Id resTy) override;
    NodeVal performOperRegular(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op) override;
    NodeVal performTuple(CodeLoc codeLoc, TypeTable::Id ty, const std::vector<NodeVal> &membs) override;

public:
    Evaluator(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompilationMessages *msgs);
};