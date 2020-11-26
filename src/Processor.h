#pragma once

#include <optional>
#include <vector>
#include "NodeVal.h"
#include "NamePool.h"
#include "StringPool.h"
#include "TypeTable.h"
#include "SymbolTable.h"
#include "CompilationMessages.h"

class Evaluator;

class Processor {
protected:
    NamePool *namePool;
    StringPool *stringPool;
    TypeTable *typeTable;
    SymbolTable *symbolTable;
    CompilationMessages *msgs;
    Evaluator *evaluator;

    unsigned topmost;

protected:
    virtual bool isSkippingProcessing() const =0;
    virtual bool isRepeatingProcessing(std::optional<NamePool::Id> block) const =0;
    virtual NodeVal performLoad(CodeLoc codeLoc, NamePool::Id id, NodeVal &ref) =0;
    virtual NodeVal performRegister(CodeLoc codeLoc, NamePool::Id id, TypeTable::Id ty) =0;
    virtual NodeVal performRegister(CodeLoc codeLoc, NamePool::Id id, const NodeVal &init) =0;
    virtual NodeVal performCast(CodeLoc codeLoc, const NodeVal &node, TypeTable::Id ty) =0;
    virtual bool performBlockSetUp(CodeLoc codeLoc, SymbolTable::Block &block) =0;
    virtual bool performBlockReentry(CodeLoc codeLoc) =0;
    virtual NodeVal performBlockTearDown(CodeLoc codeLoc, const SymbolTable::Block &block, bool success) =0;
    virtual bool performExit(CodeLoc codeLoc, const SymbolTable::Block &block, const NodeVal &cond) =0;
    virtual bool performLoop(CodeLoc codeLoc, const SymbolTable::Block &block, const NodeVal &cond) =0;
    virtual bool performPass(CodeLoc codeLoc, SymbolTable::Block &block, const NodeVal &val) =0;
    virtual NodeVal performCall(CodeLoc codeLoc, const FuncValue &func, const std::vector<NodeVal> &args) =0;
    virtual bool performFunctionDeclaration(CodeLoc codeLoc, FuncValue &func) =0;
    virtual bool performFunctionDefinition(const NodeVal &args, const NodeVal &body, FuncValue &func) =0;
    virtual bool performRet(CodeLoc codeLoc) =0;
    virtual bool performRet(CodeLoc codeLoc, const NodeVal &node) =0;
    virtual NodeVal performOperUnary(CodeLoc codeLoc, const NodeVal &oper, Oper op) =0;
    virtual NodeVal performOperUnaryDeref(CodeLoc codeLoc, const NodeVal &oper) =0;
    virtual void* performOperComparisonSetUp(CodeLoc codeLoc, std::size_t opersCnt) =0;
    // Returns nullopt in case of fail. Otherwise, returns whether the variadic comparison may exit early.
    virtual std::optional<bool> performOperComparison(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op, void *signal) =0;
    virtual NodeVal performOperComparisonTearDown(CodeLoc codeLoc, bool success, void *signal) =0;
    virtual NodeVal performOperAssignment(CodeLoc codeLoc, NodeVal &lhs, const NodeVal &rhs) =0;
    virtual NodeVal performOperIndex(CodeLoc codeLoc, NodeVal &base, const NodeVal &ind, TypeTable::Id resTy) =0;
    virtual NodeVal performOperMember(CodeLoc codeLoc, NodeVal &base, std::uint64_t ind, TypeTable::Id resTy) =0;
    virtual NodeVal performOperRegular(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op) =0;
    virtual NodeVal performTuple(CodeLoc codeLoc, TypeTable::Id ty, const std::vector<NodeVal> &membs) =0;

protected:
    bool checkInGlobalScope(CodeLoc codeLoc, bool orError);
    bool checkInLocalScope(CodeLoc codeLoc, bool orError);
    bool checkHasType(const NodeVal &node, bool orError);
    bool checkIsEvalVal(CodeLoc codeLoc, const NodeVal &node, bool orError);
    bool checkIsEvalVal(const NodeVal &node, bool orError) { return checkIsEvalVal(node.getCodeLoc(), node, orError); }
    bool checkIsLlvmVal(CodeLoc codeLoc, const NodeVal &node, bool orError);
    bool checkIsLlvmVal(const NodeVal &node, bool orError) { return checkIsLlvmVal(node.getCodeLoc(), node, orError); }
private:
    bool checkIsTopmost(CodeLoc codeLoc, bool orError);
    bool checkIsId(const NodeVal &node, bool orError);
    bool checkIsType(const NodeVal &node, bool orError);
    bool checkIsBool(const NodeVal &node, bool orError);
    bool checkIsRaw(const NodeVal &node, bool orError);
    // Checks that the node is EvalVal or LlvmVal.
    bool checkIsValue(const NodeVal &node, bool orError);
    bool checkExactlyChildren(const NodeVal &node, std::size_t n, bool orError);
    bool checkAtLeastChildren(const NodeVal &node, std::size_t n, bool orError);
    bool checkAtMostChildren(const NodeVal &node, std::size_t n, bool orError);
    bool checkBetweenChildren(const NodeVal &node, std::size_t nLo, std::size_t nHi, bool orError);
    bool checkImplicitCastable(const NodeVal &node, TypeTable::Id ty, bool orError);

    NodeVal dispatchCast(CodeLoc codeLoc, const NodeVal &node, TypeTable::Id ty);
    NodeVal dispatchOperUnaryDeref(CodeLoc codeLoc, const NodeVal &oper);

    NodeVal processAndCheckIsType(const NodeVal &node);
    NodeVal processWithEscapeIfLeaf(const NodeVal &node);
    NodeVal processWithEscapeIfLeafAndCheckIsId(const NodeVal &node);
    NodeVal processForTypeArg(const NodeVal &node);
    std::pair<NodeVal, std::optional<NodeVal>> processForIdTypePair(const NodeVal &node);
    NodeVal processAndCheckHasType(const NodeVal &node);
    NodeVal processAndImplicitCast(const NodeVal &node, TypeTable::Id ty);

    NodeVal processOperUnary(CodeLoc codeLoc, const NodeVal &oper, Oper op);
    NodeVal processOperComparison(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers, Oper op);
    NodeVal processOperAssignment(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers);
    NodeVal processOperIndex(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers);
    NodeVal processOperMember(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers);
    NodeVal processOperRegular(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers, Oper op);

protected:
    bool processChildNodes(const NodeVal &node);

private:
    NodeVal promoteLiteralVal(const NodeVal &node);
    bool applyTypeDescrDecor(TypeTable::TypeDescr &descr, const NodeVal &node);
    bool implicitCastOperands(NodeVal &lhs, NodeVal &rhs, bool oneWayOnly);

    NodeVal processInvoke(const NodeVal &node, const NodeVal &starting);
    NodeVal processType(const NodeVal &node, const NodeVal &starting);
    NodeVal processId(const NodeVal &node);
    NodeVal processSym(const NodeVal &node);
    NodeVal processCast(const NodeVal &node);
    NodeVal processBlock(const NodeVal &node);
    NodeVal processExit(const NodeVal &node);
    NodeVal processLoop(const NodeVal &node);
    NodeVal processPass(const NodeVal &node);
    NodeVal processCall(const NodeVal &node, const NodeVal &starting);
    NodeVal processFnc(const NodeVal &node);
    NodeVal processRet(const NodeVal &node);
    NodeVal processMac(const NodeVal &node);
    NodeVal processEval(const NodeVal &node);
    NodeVal processImport(const NodeVal &node);
    NodeVal processOper(const NodeVal &node, Oper op);
    NodeVal processTuple(const NodeVal &node, const NodeVal &starting);

    NodeVal processLeaf(const NodeVal &node);
    NodeVal processNonLeaf(const NodeVal &node);

public:
    Processor(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompilationMessages *msgs, Evaluator *evaluator);

    NodeVal processNode(const NodeVal &node);

    virtual ~Processor() {}
};