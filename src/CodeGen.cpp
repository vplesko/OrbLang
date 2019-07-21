#include "Parser.h"
#include "llvm/IR/Constants.h"
#include "llvm/ADT/APInt.h"
using namespace std;

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
        return nullptr;
    default:
        panic = true;
        return nullptr;
    }
}

llvm::Value* Parser::codegen(const VarExprAST *ast) {
    llvm::Value *val = symbolTable->get(ast->getNameId());
    if (val == nullptr) panic = true;
    return val;
}

llvm::Value* Parser::codegen(const BinExprAST *ast) {
    llvm::Value *valL = codegen(ast->getL());
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