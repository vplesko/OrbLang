#pragma once

#include "Processor.h"

class Evaluator : public Processor {
    friend class Processor;
    
    bool exitOrPassIssued, loopIssued;
    std::optional<NamePool::Id> skipBlock;

    bool isSkipIssued() const;
    bool isSkipIssuedForCurrBlock(std::optional<NamePool::Id> currBlockName) const;
    void resetSkipIssued();

    bool assignBasedOnTypeI(KnownVal &val, std::int64_t x, TypeTable::Id ty);
    bool assignBasedOnTypeU(KnownVal &val, std::uint64_t x, TypeTable::Id ty);
    bool assignBasedOnTypeF(KnownVal &val, double x, TypeTable::Id ty);
    bool assignBasedOnTypeC(KnownVal &val, char x, TypeTable::Id ty);
    bool assignBasedOnTypeB(KnownVal &val, bool x, TypeTable::Id ty);

    std::optional<KnownVal> makeCast(const KnownVal &srcKnownVal, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId);
    std::optional<KnownVal> makeArray(TypeTable::Id arrTypeId);

public:
    typedef bool ComparisonSignal;

    // TODO!
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
    NodeVal performCall(CodeLoc codeLoc, const FuncValue &func, const std::vector<NodeVal> &args) { msgs->errorInternal(codeLoc); return NodeVal(); }
    bool performFunctionDeclaration(CodeLoc codeLoc, FuncValue &func) { msgs->errorInternal(codeLoc); return false; }
    bool performFunctionDefinition(const NodeVal &args, const NodeVal &body, FuncValue &func) { msgs->errorInternal(body.getCodeLoc()); return false; }
    bool performRet(CodeLoc codeLoc) { msgs->errorInternal(codeLoc); return false; }
    bool performRet(CodeLoc codeLoc, const NodeVal &node) { msgs->errorInternal(codeLoc); return false; }
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
    Evaluator(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompileMessages *msgs);
};