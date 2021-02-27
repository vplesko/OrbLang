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
    Processor *compiler;

protected:
    virtual NodeVal performLoad(CodeLoc codeLoc, SymbolTable::VarEntry &ref, std::optional<NamePool::Id> id = std::nullopt) =0;
    virtual NodeVal performLoad(CodeLoc codeLoc, const FuncValue &func) =0;
    virtual NodeVal performLoad(CodeLoc codeLoc, const MacroValue &macro) =0;
    virtual NodeVal performZero(CodeLoc codeLoc, TypeTable::Id ty) =0;
    virtual NodeVal performRegister(CodeLoc codeLoc, NamePool::Id id, TypeTable::Id ty) =0;
    virtual NodeVal performRegister(CodeLoc codeLoc, NamePool::Id id, const NodeVal &init) =0;
    virtual NodeVal performCast(CodeLoc codeLoc, const NodeVal &node, TypeTable::Id ty, bool turnIntoNoDrop) =0;
    virtual bool performBlockSetUp(CodeLoc codeLoc, SymbolTable::Block &block) =0;
    // Returns nullopt in case of fail. Otherwise, returns whether the body should be processed again.
    virtual std::optional<bool> performBlockBody(CodeLoc codeLoc, SymbolTable::Block block, const NodeVal &nodeBody) =0;
    virtual NodeVal performBlockTearDown(CodeLoc codeLoc, SymbolTable::Block block, bool success) =0;
    virtual bool performExit(CodeLoc codeLoc, SymbolTable::Block block, const NodeVal &cond) =0;
    virtual bool performLoop(CodeLoc codeLoc, SymbolTable::Block block, const NodeVal &cond) =0;
    virtual bool performPass(CodeLoc codeLoc, SymbolTable::Block block, const NodeVal &val) =0;
    virtual NodeVal performCall(CodeLoc codeLoc, const NodeVal &func, const std::vector<NodeVal> &args) =0;
    virtual NodeVal performCall(CodeLoc codeLoc, const FuncValue &func, const std::vector<NodeVal> &args) =0;
    virtual NodeVal performInvoke(CodeLoc codeLoc, const MacroValue &macro, const std::vector<NodeVal> &args) =0;
    virtual bool performFunctionDeclaration(CodeLoc codeLoc, FuncValue &func) =0;
    virtual bool performFunctionDefinition(CodeLoc codeLoc, const NodeVal &args, const NodeVal &body, FuncValue &func) =0;
    virtual bool performMacroDefinition(CodeLoc codeLoc, const NodeVal &args, const NodeVal &body, MacroValue &macro) =0;
    virtual bool performRet(CodeLoc codeLoc) =0;
    virtual bool performRet(CodeLoc codeLoc, const NodeVal &node) =0;
    virtual NodeVal performOperUnary(CodeLoc codeLoc, const NodeVal &oper, Oper op) =0;
    virtual NodeVal performOperUnaryDeref(CodeLoc codeLoc, const NodeVal &oper, TypeTable::Id resTy) =0;
    virtual void* performOperComparisonSetUp(CodeLoc codeLoc, std::size_t opersCnt) =0;
    // Returns nullopt in case of fail. Otherwise, returns whether the variadic comparison may exit early.
    virtual std::optional<bool> performOperComparison(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op, void *signal) =0;
    virtual NodeVal performOperComparisonTearDown(CodeLoc codeLoc, bool success, void *signal) =0;
    virtual NodeVal performOperAssignment(CodeLoc codeLoc, NodeVal &lhs, const NodeVal &rhs) =0;
    virtual NodeVal performOperIndex(CodeLoc codeLoc, NodeVal &base, const NodeVal &ind, TypeTable::Id resTy) =0;
    virtual NodeVal performOperDot(CodeLoc codeLoc, NodeVal &base, std::uint64_t ind, TypeTable::Id resTy) =0;
    virtual NodeVal performOperRegular(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op) =0;
    virtual std::optional<std::uint64_t> performSizeOf(CodeLoc codeLoc, TypeTable::Id ty) =0;

protected:
    bool checkInGlobalScope(CodeLoc codeLoc, bool orError);
    bool checkInLocalScope(CodeLoc codeLoc, bool orError);
    bool checkHasType(const NodeVal &node, bool orError);
    bool checkIsEvalTime(CodeLoc codeLoc, const NodeVal &node, bool orError);
    bool checkIsEvalTime(const NodeVal &node, bool orError) { return checkIsEvalVal(node.getCodeLoc(), node, orError); }
    bool checkIsEvalFunc(CodeLoc codeLoc, const FuncValue &func, bool orError);
    bool checkIsEvalVal(CodeLoc codeLoc, const NodeVal &node, bool orError);
    bool checkIsEvalVal(const NodeVal &node, bool orError) { return checkIsEvalVal(node.getCodeLoc(), node, orError); }
    bool checkIsLlvmVal(CodeLoc codeLoc, const NodeVal &node, bool orError);
    bool checkIsLlvmVal(const NodeVal &node, bool orError) { return checkIsLlvmVal(node.getCodeLoc(), node, orError); }
    bool checkIsLlvmFunc(CodeLoc codeLoc, const FuncValue &func, bool orError);
    bool checkIsRaw(const NodeVal &node, bool orError);
    bool checkIsEmpty(const NodeVal &node, bool orError);
    bool checkIsId(const NodeVal &node, bool orError);
    bool checkIsType(const NodeVal &node, bool orError);
    bool checkIsBool(const NodeVal &node, bool orError);
    bool checkHasTrivialDrop(CodeLoc codeLoc, TypeTable::Id ty, bool orError);
    bool checkTransferValueOk(CodeLoc codeLoc, const NodeVal &src, bool dstNoDrop, bool orError);
    bool checkNotNeedsDrop(CodeLoc codeLoc, const NodeVal &val, bool orError);
    bool checkIsDropFuncType(const NodeVal &node, TypeTable::Id dropeeTy, bool orError);
    // Checks that the node is EvalVal or LlvmVal.
    bool checkIsValue(const NodeVal &node, bool orError);
    bool checkExactlyChildren(const NodeVal &node, std::size_t n, bool orError);
    bool checkAtLeastChildren(const NodeVal &node, std::size_t n, bool orError);
    bool checkAtMostChildren(const NodeVal &node, std::size_t n, bool orError);
    bool checkBetweenChildren(const NodeVal &node, std::size_t nLo, std::size_t nHi, bool orError);
    bool checkImplicitCastable(const NodeVal &node, TypeTable::Id ty, bool orError);
    bool checkNoArgNameDuplicates(const NodeVal &nodeArgs, const std::vector<NamePool::Id> &argNames, bool orError);

private:
    NodeVal processAndCheckIsType(const NodeVal &node);
    NodeVal processAndCheckHasType(const NodeVal &node);
    NodeVal processWithEscape(const NodeVal &node, EscapeScore amount = 1);
    NodeVal processWithEscapeAndCheckIsId(const NodeVal &node);
    NodeVal processForTypeArg(const NodeVal &node);
    std::pair<NodeVal, std::optional<NodeVal>> processForIdTypePair(const NodeVal &node);
    NodeVal processAndImplicitCast(const NodeVal &node, TypeTable::Id ty);

    NodeVal processFncType(const NodeVal &node);
    NodeVal processMacType(const NodeVal &node);
    NodeVal processOperUnary(CodeLoc codeLoc, const NodeVal &oper, Oper op);
    NodeVal processOperComparison(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers, Oper op);
    NodeVal processOperAssignment(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers);
    NodeVal processOperIndex(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers);
    NodeVal processOperDot(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers);
    NodeVal processOperRegular(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers, Oper op);

    bool processAttributes(NodeVal &node);
protected:
    bool processChildNodes(const NodeVal &node);

    std::optional<NodeVal> getAttribute(const NodeVal &node, NamePool::Id attrName);
    std::optional<NodeVal> getAttribute(const NodeVal &node, const std::string &attrStrName);
    // nullopt on error or attr not eval bool, false if not present, otherwise attribute value
    std::optional<bool> getAttributeForBool(const NodeVal &node, NamePool::Id attrName);
    // nullopt on error or attr not eval bool, false if not present, otherwise attribute value
    std::optional<bool> getAttributeForBool(const NodeVal &node, const std::string &attrStrName);
    // outer optional is nullopt on error, inner is nullopt if not present, bool is attribute value
    std::optional<std::optional<bool>> getAttributeForBoolOrNotPresent(const NodeVal &node, NamePool::Id attrName);
    // outer optional is nullopt on error, inner is nullopt if not present, bool is attribute value
    std::optional<std::optional<bool>> getAttributeForBoolOrNotPresent(const NodeVal &node, const std::string &attrStrName);
private:
    NodeVal promoteBool(CodeLoc codeLoc, bool b) const;
    NodeVal promoteLiteralVal(const NodeVal &node);
    bool canBeTypeDescrDecor(const NodeVal &node);
    bool applyTypeDescrDecor(TypeTable::TypeDescr &descr, const NodeVal &node);
    bool applyTupleMemb(TypeTable::Tuple &tup, const NodeVal &node);
    NodeVal dispatchLoad(CodeLoc codeLoc, SymbolTable::VarEntry &ref, std::optional<NamePool::Id> id = std::nullopt);
    NodeVal implicitCast(const NodeVal &node, TypeTable::Id ty, bool turnIntoNoDrop = false);
    NodeVal castNode(CodeLoc codeLoc, const NodeVal &node, TypeTable::Id ty, bool turnIntoNoDrop = false);
    bool implicitCastOperands(NodeVal &lhs, NodeVal &rhs, bool oneWayOnly);
    bool shouldNotDispatchCastToEval(const NodeVal &node, TypeTable::Id dstTypeId) const;
    NodeVal dispatchCall(CodeLoc codeLoc, const NodeVal &func, const std::vector<NodeVal> &args, bool allArgsEval);
    NodeVal dispatchCall(CodeLoc codeLoc, const FuncValue &func, const std::vector<NodeVal> &args, bool allArgsEval);
    NodeVal dispatchOperUnaryDeref(CodeLoc codeLoc, const NodeVal &oper);
    NodeVal dispatchAssignment(CodeLoc codeLoc, NodeVal &lhs, const NodeVal &rhs);
    NodeVal getElement(CodeLoc codeLoc, NodeVal &array, std::size_t index);
    NodeVal getElement(CodeLoc codeLoc, NodeVal &array, const NodeVal &index);
    NodeVal getRawMember(CodeLoc codeLoc, NodeVal &raw, std::size_t index);
    NodeVal getTupleMember(CodeLoc codeLoc, NodeVal &tuple, std::size_t index);
    NodeVal getDataMember(CodeLoc codeLoc, NodeVal &data, std::size_t index);
    bool argsFitFuncCall(const std::vector<NodeVal> &args, const TypeTable::Callable &callable, bool allowImplicitCasts);
    NodeVal loadUndecidedCallable(const NodeVal &node, const NodeVal &val);
    NodeVal moveNode(CodeLoc codeLoc, NodeVal &val);
    NodeVal invoke(CodeLoc codeLoc, const MacroValue &macroVal, std::vector<NodeVal> args);
    bool hasTrivialDrop(TypeTable::Id ty);
    bool callDropFunc(CodeLoc codeLoc, NodeVal val);
    bool callDropFuncNonRef(NodeVal val);
    bool callDropFuncsNonRef(std::vector<NodeVal> val);
    bool callDropFuncs(CodeLoc codeLoc, std::vector<SymbolTable::VarEntry*> vars);
protected:
    bool callDropFuncsCurrBlock(CodeLoc codeLoc);
    bool callDropFuncsFromBlockToCurrBlock(CodeLoc codeLoc, NamePool::Id name);
    bool callDropFuncsCurrCallable(CodeLoc codeLoc);

private:
    NodeVal processType(const NodeVal &node, const NodeVal &starting);
    NodeVal processId(const NodeVal &node);
    NodeVal processId(const NodeVal &node, const NodeVal &starting);
    NodeVal processSym(const NodeVal &node);
    NodeVal processCast(const NodeVal &node);
    NodeVal processBlock(const NodeVal &node);
    NodeVal processExit(const NodeVal &node);
    NodeVal processLoop(const NodeVal &node);
    NodeVal processPass(const NodeVal &node);
    NodeVal processCustom(const NodeVal &node, const NodeVal &starting);
    NodeVal processData(const NodeVal &node, const NodeVal &starting);
    NodeVal processCall(const NodeVal &node, const NodeVal &starting);
    NodeVal processInvoke(const NodeVal &node, const NodeVal &starting);
    NodeVal processFnc(const NodeVal &node, const NodeVal &starting);
    NodeVal processMac(const NodeVal &node, const NodeVal &starting);
    NodeVal processRet(const NodeVal &node);
    NodeVal processEval(const NodeVal &node);
    NodeVal processImport(const NodeVal &node, bool topmost);
    NodeVal processMessage(const NodeVal &node);
    NodeVal processOper(const NodeVal &node, Oper op);
    NodeVal processTypeOf(const NodeVal &node);
    NodeVal processLenOf(const NodeVal &node);
    NodeVal processSizeOf(const NodeVal &node);
    NodeVal processIsDef(const NodeVal &node);
    NodeVal processAttrOf(const NodeVal &node);
    NodeVal processAttrIsDef(const NodeVal &node);

    NodeVal processLeaf(const NodeVal &node);
    NodeVal processNonLeaf(const NodeVal &node, bool topmost = false);
    NodeVal processNonLeafEscaped(const NodeVal &node);

public:
    Processor(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompilationMessages *msgs);

    void setEvaluator(Evaluator *evaluator) { this->evaluator = evaluator; }
    void setCompiler(Processor *compiler) { this->compiler = compiler; }

    NodeVal processNode(const NodeVal &node, bool topmost = false);

    virtual ~Processor() {}
};