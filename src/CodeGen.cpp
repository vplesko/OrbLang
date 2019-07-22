#include "Parser.h"
#include "llvm/IR/Constants.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/Verifier.h"
using namespace std;

llvm::AllocaInst* createEntryAlloca(llvm::Function *func, llvm::Type *type, const string &name) {
    // TODO: this inverts the order of vars. is this consequential (eg. for padding optimizations)?
    llvm::IRBuilder<> builder(&func->getEntryBlock(), func->getEntryBlock().begin());
    return builder.CreateAlloca(type, 0, name);
}

llvm::Value* Parser::codegen(const BaseAST *ast) {
    switch (ast->type()) {
    case AST_LiteralExpr:
        return llvm::ConstantInt::get(
            llvm::IntegerType::getInt64Ty(llvmContext), 
            llvm::APInt(64, ((LiteralExprAST*)ast)->getVal(), true));
    case AST_VarExpr:
        return codegen((VarExprAST*)ast);
    case AST_BinExpr:
        return codegen((BinExprAST*)ast);
    case AST_Decl:
        return codegen((DeclAST*)ast);
    default:
        panic = true;
        return nullptr;
    }
}

llvm::Value* Parser::codegen(const VarExprAST *ast) {
    llvm::Value *val = symbolTable->get(ast->getNameId());
    if (val == nullptr) { panic = true; return nullptr; }
    return llvmBuilder.CreateLoad(val, lex->getNamePool()->get(ast->getNameId()));
}

llvm::Value* Parser::codegen(const BinExprAST *ast) {
    llvm::Value *valL;
    if (ast->getOp() == Token::O_ASGN) {
        if (ast->getL()->type() != AST_VarExpr) {
            panic = true;
            return nullptr;
        }
        const VarExprAST *var = (const VarExprAST*)ast->getL();
        valL = symbolTable->get(var->getNameId());
    } else {
        valL = codegen(ast->getL());
    }
    if (panic || valL == nullptr) {
        panic = true;
        return nullptr;
    }
    llvm::Value *valR = codegen(ast->getR());
    if (panic || valR == nullptr) {
        panic = true;
        return nullptr;
    }

    switch (ast->getOp()) {
        case Token::O_ASGN:
            llvmBuilder.CreateStore(valR, valL);
            return valR;
        case Token::O_ADD:
            return llvmBuilder.CreateAdd(valL, valR, "addtmp");
        case Token::O_SUB:
            return llvmBuilder.CreateSub(valL, valR, "subtmp");
        case Token::O_MUL:
            return llvmBuilder.CreateMul(valL, valR, "multmp");
        case Token::O_DIV:
            return llvmBuilder.CreateSDiv(valL, valR, "divtmp");
        default:
            panic = true;
            return nullptr;
    }
}

llvm::Value* Parser::codegen(const DeclAST *ast) {
    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    for (const auto &it : ast->getDecls()) {
        if (symbolTable->get(it.first) != nullptr) {
            panic = true;
            return nullptr;
        }

        const string &name = lex->getNamePool()->get(it.first);
        const ExprAST *init = it.second.get();

        llvm::Value *initVal = codegen(init);
        if (panic || initVal == nullptr) return nullptr;

        llvm::AllocaInst *alloca = createEntryAlloca(
            func, llvm::IntegerType::getInt64Ty(llvmContext), name);
        llvmBuilder.CreateStore(initVal, alloca);

        symbolTable->add(it.first, alloca);
    }

    return nullptr;
}

void Parser::codegenStart() {
    llvm::FunctionType *funcTy = llvm::FunctionType::get(llvm::Type::getVoidTy(llvmContext), false);
    main = llvm::Function::Create(funcTy, llvm::Function::ExternalLinkage, "main", *llvmModule);

    llvm::BasicBlock *block = llvm::BasicBlock::Create(llvmContext, "entry", main);
    llvmBuilder.SetInsertPoint(block);
}

void Parser::codegenEnd() {
    llvm::verifyFunction(*main);
}