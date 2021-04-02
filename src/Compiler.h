#pragma once

#include <string>
#include "Processor.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

class Compiler : public Processor {
    llvm::LLVMContext llvmContext;
    llvm::IRBuilder<> llvmBuilder, llvmBuilderAlloca;
    std::unique_ptr<llvm::Module> llvmModule;
    std::unique_ptr<llvm::PassManagerBuilder> llvmPmb;
    std::unique_ptr<llvm::legacy::FunctionPassManager> llvmFpm;
    llvm::TargetMachine *targetMachine;

    bool initLlvmTargetMachine();

    bool isLlvmBlockTerminated() const;
    llvm::Function* getLlvmCurrFunction() { return llvmBuilder.GetInsertBlock()->getParent(); }
    llvm::Constant* getLlvmConstB(bool val);
    // generates a constant for a string literal
    llvm::Constant* makeLlvmConstString(const std::string &str);
    llvm::FunctionType* makeLlvmFunctionType(TypeTable::Id typeId);
    llvm::Type* makeLlvmType(TypeTable::Id typeId);
    llvm::Type* makeLlvmPrimType(TypeTable::PrimIds primTypeId) { return makeLlvmType(typeTable->getPrimTypeId(primTypeId)); }
    llvm::Type* makeLlvmTypeOrError(CodeLoc codeLoc, TypeTable::Id typeId);
    llvm::Constant* makeLlvmZero(TypeTable::Id typeId);
    llvm::Constant* makeLlvmZero(llvm::Type *llvmType, TypeTable::Id typeId);
    llvm::GlobalValue* makeLlvmGlobal(llvm::Type *type, llvm::Constant *init, bool isConstant, const std::string &name);
    llvm::AllocaInst* makeLlvmAlloca(llvm::Type *type, const std::string &name);
    llvm::Value* makeLlvmCast(llvm::Value *srcLlvmVal, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId);
    llvm::Value* makeLlvmCast(llvm::Value *srcLlvmVal, TypeTable::Id srcTypeId, llvm::Type *dstLlvmType, TypeTable::Id dstTypeId);

    std::string getNameForLlvm(NamePool::Id name) const;
    // handles name mangling
    std::optional<std::string> getFuncNameForLlvm(const FuncValue &func);

    NodeVal promoteEvalVal(CodeLoc codeLoc, const EvalVal &eval);
    NodeVal promoteEvalVal(const NodeVal &node);
    NodeVal promoteIfEvalValAndCheckIsLlvmVal(const NodeVal &node, bool orError);

    bool doCondBlockJump(CodeLoc codeLoc, const NodeVal &cond, std::optional<NamePool::Id> blockName, llvm::BasicBlock *llvmBlock);

    NodeVal performLoad(CodeLoc codeLoc, VarId varId) override;
    NodeVal performLoad(CodeLoc codeLoc, FuncId funcId) override;
    NodeVal performLoad(CodeLoc codeLoc, MacroId macroId) override;
    NodeVal performZero(CodeLoc codeLoc, TypeTable::Id ty) override;
    NodeVal performRegister(CodeLoc codeLoc, NamePool::Id id, CodeLoc codeLocTy, TypeTable::Id ty) override;
    NodeVal performRegister(CodeLoc codeLoc, NamePool::Id id, const NodeVal &init) override;
    NodeVal performCast(CodeLoc codeLoc, const NodeVal &node, CodeLoc codeLocTy, TypeTable::Id ty) override;
    bool performBlockSetUp(CodeLoc codeLoc, SymbolTable::Block &block) override;
    std::optional<bool> performBlockBody(CodeLoc codeLoc, SymbolTable::Block block, const NodeVal &nodeBody) override;
    NodeVal performBlockTearDown(CodeLoc codeLoc, SymbolTable::Block block, bool success) override;
    bool performExit(CodeLoc codeLoc, SymbolTable::Block block, const NodeVal &cond) override;
    bool performLoop(CodeLoc codeLoc, SymbolTable::Block block, const NodeVal &cond) override;
    bool performPass(CodeLoc codeLoc, SymbolTable::Block block, const NodeVal &val) override;
    NodeVal performCall(CodeLoc codeLoc, const NodeVal &func, const std::vector<NodeVal> &args) override;
    NodeVal performCall(CodeLoc codeLoc, FuncId funcId, const std::vector<NodeVal> &args) override;
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
    NodeVal performOperAssignment(CodeLoc codeLoc, NodeVal &lhs, const NodeVal &rhs) override;
    NodeVal performOperIndex(CodeLoc codeLoc, NodeVal &base, const NodeVal &ind, TypeTable::Id resTy) override;
    NodeVal performOperDot(CodeLoc codeLoc, NodeVal &base, std::uint64_t ind, TypeTable::Id resTy) override;
    NodeVal performOperRegular(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op, bool bare) override;
    std::optional<std::uint64_t> performSizeOf(CodeLoc codeLoc, TypeTable::Id ty) override;

public:
    Compiler(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompilationMessages *msgs);

    llvm::Type* genPrimTypeBool();
    llvm::Type* genPrimTypeI(unsigned bits);
    llvm::Type* genPrimTypeU(unsigned bits);
    llvm::Type* genPrimTypeC(unsigned bits);
    llvm::Type* genPrimTypeF32();
    llvm::Type* genPrimTypeF64();
    llvm::Type* genPrimTypePtr();

    void setOptLevel(unsigned lvl);

    void printout(const std::string &filename) const;
    bool binary(const std::string &filename);
};