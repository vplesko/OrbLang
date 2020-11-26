#pragma once

#include "Processor.h"

class Evaluator : public Processor {
    friend class Processor;
    
    bool exitOrPassIssued, loopIssued;
    std::optional<NamePool::Id> skipBlock;

    bool retIssued;
    std::optional<NodeVal> retVal;

    bool isSkipIssuedNotRet() const { return exitOrPassIssued || loopIssued; }
    bool isSkipIssued() const { return isSkipIssuedNotRet() || retIssued; }
    bool isSkipIssuedForCurrBlock(std::optional<NamePool::Id> currBlockName) const;
    void resetSkipIssued();

    bool assignBasedOnTypeI(EvalVal &val, std::int64_t x, TypeTable::Id ty);
    bool assignBasedOnTypeU(EvalVal &val, std::uint64_t x, TypeTable::Id ty);
    bool assignBasedOnTypeF(EvalVal &val, double x, TypeTable::Id ty);
    bool assignBasedOnTypeC(EvalVal &val, char x, TypeTable::Id ty);
    bool assignBasedOnTypeB(EvalVal &val, bool x, TypeTable::Id ty);

    std::optional<EvalVal> makeCast(const EvalVal &srcEvalVal, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId);
    std::optional<EvalVal> makeArray(TypeTable::Id arrTypeId);

public:
    bool isSkippingProcessing() const { return isSkipIssued(); }
    bool isRepeatingProcessing(std::optional<NamePool::Id> block) const;
    NodeVal performLoad(CodeLoc codeLoc, NamePool::Id id, NodeVal &ref);
    NodeVal performRegister(CodeLoc codeLoc, NamePool::Id id, TypeTable::Id ty);
    NodeVal performRegister(CodeLoc codeLoc, NamePool::Id id, const NodeVal &init);
    NodeVal performCast(CodeLoc codeLoc, const NodeVal &node, TypeTable::Id ty);
    bool performBlockSetUp(CodeLoc codeLoc, SymbolTable::Block &block) { return true; }
    bool performBlockReentry(CodeLoc codeLoc);
    NodeVal performBlockTearDown(CodeLoc codeLoc, const SymbolTable::Block &block, bool success);
    bool performExit(CodeLoc codeLoc, const SymbolTable::Block &block, const NodeVal &cond);
    bool performLoop(CodeLoc codeLoc, const SymbolTable::Block &block, const NodeVal &cond);
    bool performPass(CodeLoc codeLoc, SymbolTable::Block &block, const NodeVal &val);
    NodeVal performCall(CodeLoc codeLoc, const FuncValue &func, const std::vector<NodeVal> &args);
    bool performFunctionDeclaration(CodeLoc codeLoc, FuncValue &func);
    bool performFunctionDefinition(const NodeVal &args, const NodeVal &body, FuncValue &func);
    bool performRet(CodeLoc codeLoc);
    bool performRet(CodeLoc codeLoc, const NodeVal &node);
    NodeVal performOperUnary(CodeLoc codeLoc, const NodeVal &oper, Oper op);
    NodeVal performOperUnaryDeref(CodeLoc codeLoc, const NodeVal &oper);
    void* performOperComparisonSetUp(CodeLoc codeLoc, std::size_t opersCnt);
    std::optional<bool> performOperComparison(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op, void *signal);
    NodeVal performOperComparisonTearDown(CodeLoc codeLoc, bool success, void *signal);
    NodeVal performOperAssignment(CodeLoc codeLoc, NodeVal &lhs, const NodeVal &rhs);
    NodeVal performOperIndex(CodeLoc codeLoc, NodeVal &base, const NodeVal &ind, TypeTable::Id resTy);
    NodeVal performOperMember(CodeLoc codeLoc, NodeVal &base, std::uint64_t ind, TypeTable::Id resTy);
    NodeVal performOperRegular(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op);
    NodeVal performTuple(CodeLoc codeLoc, TypeTable::Id ty, const std::vector<NodeVal> &membs);

public:
    Evaluator(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompilationMessages *msgs);
};