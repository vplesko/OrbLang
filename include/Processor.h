#pragma once

#include <optional>
#include <vector>
#include "NodeVal.h"
#include "NamePool.h"
#include "StringPool.h"
#include "TypeTable.h"
#include "SymbolTable.h"
#include "CompileMessages.h"

class Processor {
protected:
    NamePool *namePool;
    StringPool *stringPool;
    TypeTable *typeTable;
    SymbolTable *symbolTable;
    CompileMessages *msgs;

protected:
    virtual NodeVal performLoad(CodeLoc codeLoc, NamePool::Id id, const NodeVal &val) =0;
    virtual NodeVal performRegister(CodeLoc codeLoc, NamePool::Id id, TypeTable::Id ty) =0;
    virtual NodeVal performRegister(CodeLoc codeLoc, NamePool::Id id, const NodeVal &init) =0;
    virtual NodeVal performCast(CodeLoc codeLoc, const NodeVal &node, TypeTable::Id ty) =0;
    virtual NodeVal performCall(CodeLoc codeLoc, const FuncValue &func, const std::vector<NodeVal> &args) =0;
    virtual bool performFunctionDeclaration(CodeLoc codeLoc, FuncValue &func) =0;
    virtual bool performFunctionDefinition(const NodeVal &args, const NodeVal &body, FuncValue &func) =0;
    virtual bool performRet(CodeLoc codeLoc) =0;
    virtual bool performRet(CodeLoc codeLoc, const NodeVal &node) =0;
    virtual NodeVal performEvaluation(const NodeVal &node) =0;
    virtual NodeVal performOperUnary(CodeLoc codeLoc, const NodeVal &oper, Oper op) =0;
    virtual NodeVal performOperUnaryDeref(CodeLoc codeLoc, const NodeVal &oper) =0;
    virtual void* performOperComparisonSetUp() =0;
    virtual bool performOperComparison(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op, void *signal) =0;
    virtual NodeVal performOperComparisonTearDown(CodeLoc codeLoc, void *signal) =0;
    virtual NodeVal performOperAssignment(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs) =0;
    virtual NodeVal performOperMember(CodeLoc codeLoc, const NodeVal &base, const NodeVal &ind) =0;
    virtual NodeVal performOperRegular(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op) =0;
    virtual NodeVal performTuple(CodeLoc codeLoc, TypeTable::Id ty, const std::vector<NodeVal> &membs) =0;

protected:
    bool checkInGlobalScope(CodeLoc codeLoc, bool orError);
    bool checkInLocalScope(CodeLoc codeLoc, bool orError);
private:
    bool checkIsId(const NodeVal &node, bool orError);
    bool checkIsType(const NodeVal &node, bool orError);
    bool checkIsComposite(const NodeVal &node, bool orError);
    // Checks that the node is KnownVal or LlvmVal.
    bool checkIsValue(const NodeVal &node, bool orError);
    bool checkExactlyChildren(const NodeVal &node, std::size_t n, bool orError);
    bool checkAtLeastChildren(const NodeVal &node, std::size_t n, bool orError);
    bool checkAtMostChildren(const NodeVal &node, std::size_t n, bool orError);
    bool checkBetweenChildren(const NodeVal &node, std::size_t nLo, std::size_t nHi, bool orError);
    bool checkImplicitCastable(const NodeVal &node, TypeTable::Id ty, bool orError);

    NodeVal processAndExpectType(const NodeVal &node);
    NodeVal processWithEscapeIfLeaf(const NodeVal &node);
    NodeVal processWithEscapeIfLeafAndExpectId(const NodeVal &node);
    NodeVal processWithEscapeIfLeafUnlessType(const NodeVal &node);
    std::pair<NodeVal, std::optional<NodeVal>> processForIdTypePair(const NodeVal &node);
    NodeVal processAndCheckIsValue(const NodeVal &node);
    NodeVal processAndImplicitCast(const NodeVal &node, TypeTable::Id ty);

    NodeVal processOperUnary(CodeLoc codeLoc, const NodeVal &oper, Oper op);
    NodeVal processOperComparison(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers, Oper op);
    NodeVal processOperAssignment(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers);
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
    Processor(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompileMessages *msgs);

    // TODO check for is skipping processing
    NodeVal processNode(const NodeVal &node);

    virtual ~Processor() {}
};