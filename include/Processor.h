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
    virtual NodeVal loadSymbol(NamePool::Id id) =0;
    // TODO don't forget str to char arr
    virtual NodeVal cast(const NodeVal &node, TypeTable::Id ty) =0;
    virtual bool makeFunction(const NodeVal &node, FuncValue &func) =0;
    virtual NodeVal createCall(const FuncValue &func, const std::vector<NodeVal> &args) =0;
    virtual NodeVal evaluateNode(const NodeVal &node) =0;

private:
    bool checkInGlobalScope(CodeLoc codeLoc, bool orError);
    bool checkIsId(const NodeVal &node, bool orError);
    bool checkIsType(const NodeVal &node, bool orError);
    bool checkIsComposite(const NodeVal &node, bool orError);
    bool checkExactlyChildren(const NodeVal &node, std::size_t n, bool orError);
    bool checkAtLeastChildren(const NodeVal &node, std::size_t n, bool orError);
    bool checkAtMostChildren(const NodeVal &node, std::size_t n, bool orError);
    bool checkBetweenChildren(const NodeVal &node, std::size_t nLo, std::size_t nHi, bool orError);

    NodeVal processAndExpectType(const NodeVal &node);
    NodeVal processWithEscapeIfLeaf(const NodeVal &node);
    NodeVal processWithEscapeIfLeafAndExpectId(const NodeVal &node);
    NodeVal processWithEscapeIfLeafUnlessType(const NodeVal &node);
    std::pair<NodeVal, std::optional<NodeVal>> processForIdTypePair(const NodeVal &node);

    NodeVal promoteLiteralVal(const NodeVal &node);
    bool applyTypeDescrDecor(TypeTable::TypeDescr &descr, const NodeVal &node);

    NodeVal processInvoke(const NodeVal &node, const NodeVal &starting);
    NodeVal processType(const NodeVal &node, const NodeVal &starting);
    NodeVal processCall(const NodeVal &node, const NodeVal &starting);
    // TODO don't forget types, funcs, macros are vars
    NodeVal processId(const NodeVal &node);
    NodeVal processSym(const NodeVal &node);
    NodeVal processCast(const NodeVal &node);
    NodeVal processBlock(const NodeVal &node);
    NodeVal processExit(const NodeVal &node);
    NodeVal processLoop(const NodeVal &node);
    NodeVal processPass(const NodeVal &node);
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