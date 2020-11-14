#pragma once

#include <string>
#include "Processor.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

class Codegen : public Processor {
    llvm::LLVMContext llvmContext;
    llvm::IRBuilder<> llvmBuilder, llvmBuilderAlloca;
    std::unique_ptr<llvm::Module> llvmModule;
    std::unique_ptr<llvm::PassManagerBuilder> llvmPmb;
    std::unique_ptr<llvm::legacy::FunctionPassManager> llvmFpm;

    bool isLlvmBlockTerminated() const;
    llvm::Function* getLlvmCurrFunction() { return llvmBuilder.GetInsertBlock()->getParent(); }
    llvm::Constant* getLlvmConstB(bool val);
    // generates a constant for a string literal
    llvm::Constant* makeLlvmConstString(const std::string &str);
    llvm::Type* makeLlvmType(TypeTable::Id typeId);
    llvm::Type* makeLlvmPrimType(TypeTable::PrimIds primTypeId) { return makeLlvmType(typeTable->getPrimTypeId(primTypeId)); }
    llvm::Type* makeLlvmTypeOrError(CodeLoc codeLoc, TypeTable::Id typeId);
    llvm::GlobalValue* makeLlvmGlobal(llvm::Type *type, llvm::Constant *init, bool isConstant, const std::string &name);
    llvm::AllocaInst* makeLlvmAlloca(llvm::Type *type, const std::string &name);
    llvm::Value* makeLlvmCast(llvm::Value *srcLlvmVal, TypeTable::Id srcTypeId, llvm::Type *dstLlvmType, TypeTable::Id dstTypeId);

    std::string getNameForLlvm(NamePool::Id name) const;

    NodeVal promoteKnownVal(CodeLoc codeLoc, const KnownVal &known);
    NodeVal promoteKnownVal(const NodeVal &node);
    NodeVal promoteIfKnownValAndCheckIsLlvmVal(const NodeVal &node, bool orError);

    bool isSkippingProcessing() const { return false; }
    bool isRepeatingProcessing(std::optional<NamePool::Id> block) const { return false; }
    NodeVal performLoad(CodeLoc codeLoc, NamePool::Id id, NodeVal &ref);
    NodeVal performRegister(CodeLoc codeLoc, NamePool::Id id, TypeTable::Id ty);
    NodeVal performRegister(CodeLoc codeLoc, NamePool::Id id, const NodeVal &init);
    NodeVal performCast(CodeLoc codeLoc, const NodeVal &node, TypeTable::Id ty);
    bool performBlockSetUp(CodeLoc codeLoc, SymbolTable::Block &block);
    bool performBlockReentry(CodeLoc codeLoc) { return true; }
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
    Codegen(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompileMessages *msgs, Evaluator *evaluator);

    llvm::Type* genPrimTypeBool();
    llvm::Type* genPrimTypeI(unsigned bits);
    llvm::Type* genPrimTypeU(unsigned bits);
    llvm::Type* genPrimTypeC(unsigned bits);
    llvm::Type* genPrimTypeF32();
    llvm::Type* genPrimTypeF64();
    llvm::Type* genPrimTypePtr();

    void printout() const;
    bool binary(const std::string &filename);
};