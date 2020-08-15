#pragma once

#include <stack>
#include <optional>
#include "CodeProcessor.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

class Evaluator;

class Codegen : public CodeProcessor {
    Evaluator *evaluator;

    llvm::LLVMContext llvmContext;
    llvm::IRBuilder<> llvmBuilder, llvmBuilderAlloca;
    std::unique_ptr<llvm::Module> llvmModule;
    std::unique_ptr<llvm::PassManagerBuilder> llvmPMB;
    std::unique_ptr<llvm::legacy::FunctionPassManager> llvmFPM;

    bool promoteKnownVal(NodeVal &e, TypeTable::Id t);
    bool promoteKnownVal(NodeVal &e);

    llvm::Value* getConstB(bool val);

    llvm::AllocaInst* createAlloca(llvm::Type *type, const std::string &name);
    llvm::GlobalValue* createGlobal(llvm::Type *type, llvm::Constant *init, bool isConstant, const std::string &name);
    llvm::Constant* createString(const std::string &str);
    
    bool mangleName(std::stringstream &ss, TypeTable::Id ty);
    std::optional<NamePool::Id> mangleName(const FuncValue &f);

    llvm::Type* getLlvmType(TypeTable::Id typeId);
    bool createCast(llvm::Value *&val, TypeTable::Id srcTypeId, llvm::Type *type, TypeTable::Id dstTypeId);
    bool createCast(llvm::Value *&val, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId);
    // TODO make the arg immutable; have dependant funcs take const refs; same for Evaluator
    bool createCast(NodeVal &e, TypeTable::Id t);

    bool isLlvmBlockTerminated() const;

    NodeVal handleImplicitConversion(CodeLoc codeLoc, NodeVal val, TypeTable::Id t);
    NodeVal handleImplicitConversion(const AstNode *ast, TypeTable::Id t);

    std::optional<FuncValue> calculateFuncProto(const AstNode *ast, bool definition);

    bool isSkippingProcessing() const { return false; }

    NodeVal processKnownVal(const AstNode *ast);
    NodeVal processType(const AstNode *ast, const NodeVal &first);
    NodeVal processVar(const AstNode *ast);
    NodeVal processOperUnary(const AstNode *ast, const NodeVal &first);
    NodeVal processOperInd(CodeLoc codeLoc, const NodeVal &base, const NodeVal &ind);
    NodeVal processOperInd(const AstNode *ast);
    NodeVal processOperDot(CodeLoc codeLoc, const NodeVal &base, const AstNode *astMemb);
    NodeVal processOperDot(const AstNode *ast);
    NodeVal handleOper(CodeLoc codeLoc, Token::Oper op, const NodeVal &lhs, const NodeVal &rhs);
    NodeVal processTuple(const AstNode *ast, const NodeVal &first);
    NodeVal processCall(const AstNode *ast, const NodeVal &first);
    NodeVal processCast(const AstNode *ast);
    NodeVal processArr(const AstNode *ast);
    NodeVal processSym(const AstNode *ast);
    void handleExit(const SymbolTable::Block *targetBlock, CodeLoc condCodeLoc, NodeVal cond);
    void handleLoop(const SymbolTable::Block *targetBlock, CodeLoc condCodeLoc, NodeVal cond);
    void handlePass(const SymbolTable::Block *targetBlock, CodeLoc valCodeLoc, NodeVal val, TypeTable::Id expectedType);
    NodeVal handleBlock(std::optional<NamePool::Id> name, std::optional<TypeTable::Id> type, const AstNode *nodeBody);
    NodeVal processBlock(const AstNode *ast);
    NodeVal processRet(const AstNode *ast);
    NodeVal processFunc(const AstNode *ast);
    NodeVal processMac(AstNode *ast);
    NodeVal processEvalNode(const AstNode *ast);
    std::unique_ptr<AstNode> processInvoke(NamePool::Id macroName, const AstNode *ast);

public:
    Codegen(NamePool *namePool, StringPool *stringPool, SymbolTable *symbolTable, CompileMessages *msgs);

    void setEvaluator(Evaluator *e) { evaluator = e; }

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