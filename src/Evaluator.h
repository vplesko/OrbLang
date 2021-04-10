#pragma once

#include "Processor.h"

class Evaluator : public Processor {
    friend class Processor;

    // TODO put this into an exception?
    std::optional<NodeVal> retVal;

    bool assignBasedOnTypeI(EvalVal &val, std::int64_t x, TypeTable::Id ty);
    bool assignBasedOnTypeU(EvalVal &val, std::uint64_t x, TypeTable::Id ty);
    bool assignBasedOnTypeF(EvalVal &val, double x, TypeTable::Id ty);
    bool assignBasedOnTypeC(EvalVal &val, char x, TypeTable::Id ty);
    bool assignBasedOnTypeB(EvalVal &val, bool x, TypeTable::Id ty);
    bool assignBasedOnTypeP(EvalVal &val, EvalVal::Pointer x, TypeTable::Id ty);
    bool assignBasedOnTypeId(EvalVal &val, std::uint64_t x, TypeTable::Id ty);
    bool assignBasedOnTypeId(EvalVal &val, bool x, TypeTable::Id ty);

    std::optional<NodeVal> makeCast(CodeLoc codeLoc, const NodeVal &srcVal, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId);
    std::optional<EvalVal> makeArray(TypeTable::Id arrTypeId);
    NamePool::Id makeIdFromU(std::uint64_t x);
    NamePool::Id makeIdFromB(bool x);
    std::optional<NamePool::Id> makeIdFromTy(TypeTable::Id x);
    NamePool::Id makeIdConcat(NamePool::Id lhs, NamePool::Id rhs, bool bare);
    std::vector<NodeVal> makeRawConcat(const EvalVal &lhs, const EvalVal &rhs) const;

    NodeVal doBlockTearDown(CodeLoc codeLoc, SymbolTable::Block block, bool success, bool jumpingOut);

public:
    NodeVal performLoad(CodeLoc codeLoc, VarId varId) override;
    NodeVal performLoad(CodeLoc codeLoc, FuncId funcId) override;
    NodeVal performLoad(CodeLoc codeLoc, MacroId macroId) override;
    NodeVal performZero(CodeLoc codeLoc, TypeTable::Id ty) override;
    NodeVal performRegister(CodeLoc codeLoc, NamePool::Id id, CodeLoc codeLocTy, TypeTable::Id ty) override;
    NodeVal performRegister(CodeLoc codeLoc, NamePool::Id id, const NodeVal &init) override;
    NodeVal performCast(CodeLoc codeLoc, const NodeVal &node, CodeLoc codeLocTy, TypeTable::Id ty) override;
    bool performBlockSetUp(CodeLoc codeLoc, SymbolTable::Block &block) override { return true; }
    std::optional<bool> performBlockBody(CodeLoc codeLoc, SymbolTable::Block block, const NodeVal &nodeBody) override;
    NodeVal performBlockTearDown(CodeLoc codeLoc, SymbolTable::Block block, bool success) override;
    bool performExit(CodeLoc codeLoc, SymbolTable::Block block, const NodeVal &cond) override;
    bool performLoop(CodeLoc codeLoc, SymbolTable::Block block, const NodeVal &cond) override;
    bool performPass(CodeLoc codeLoc, SymbolTable::Block block, const NodeVal &val) override;
    NodeVal performCall(CodeLoc codeLoc, CodeLoc codeLocFunc, const NodeVal &func, const std::vector<NodeVal> &args) override;
    NodeVal performCall(CodeLoc codeLoc, CodeLoc codeLocFunc, FuncId funcId, const std::vector<NodeVal> &args) override;
    NodeVal performInvoke(CodeLoc codeLoc, MacroId macroId, const std::vector<NodeVal> &args) override;
    bool performFunctionDeclaration(CodeLoc codeLoc, FuncValue &func) override;
    bool performFunctionDefinition(CodeLoc codeLoc, const NodeVal &args, const NodeVal &body, FuncValue &func) override;
    bool performMacroDefinition(CodeLoc codeLoc, const NodeVal &args, const NodeVal &body, MacroValue &macro) override;
    bool performRet(CodeLoc codeLoc) override;
    bool performRet(CodeLoc codeLoc, const NodeVal &node) override;
    NodeVal performOperUnary(CodeLoc codeLoc, const NodeVal &oper, Oper op) override;
    NodeVal performOperUnaryDeref(CodeLoc codeLoc, const NodeVal &oper, TypeTable::Id resTy) override;
    ComparisonSignal performOperComparisonSetUp(CodeLoc codeLoc, std::size_t opersCnt) override;
    std::optional<bool> performOperComparison(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op, ComparisonSignal &signal) override;
    NodeVal performOperComparisonTearDown(CodeLoc codeLoc, bool success, ComparisonSignal signal) override;
    NodeVal performOperAssignment(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs) override;
    NodeVal performOperIndexArr(CodeLoc codeLoc, NodeVal &base, const NodeVal &ind, TypeTable::Id resTy) override;
    NodeVal performOperIndex(CodeLoc codeLoc, NodeVal &base, std::uint64_t ind, TypeTable::Id resTy) override;
    NodeVal performOperRegular(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op, bool bare) override;
    std::optional<std::uint64_t> performSizeOf(CodeLoc codeLoc, TypeTable::Id ty) override;

public:
    Evaluator(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompilationMessages *msgs);
};