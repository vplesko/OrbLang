#include "Codegen.h"
#include "Parser.h"
#include "llvm/IR/Constants.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/Verifier.h"
using namespace std;

CodeGen::CodeGen(const NamePool *namePool, SymbolTable *symbolTable) : namePool(namePool), symbolTable(symbolTable), 
        llvmBuilder(llvmContext), llvmBuilderAlloca(llvmContext), panic(false) {
    llvmModule = std::make_unique<llvm::Module>(llvm::StringRef("test"), llvmContext);
    funcBody = nullptr;
    main = nullptr;
}

llvm::AllocaInst* CodeGen::createAlloca(const string &name) {
    return llvmBuilderAlloca.CreateAlloca(llvm::IntegerType::getInt64Ty(llvmContext), 0, name);
}

llvm::Value* CodeGen::codegen(const BaseAST *ast) {
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
    case AST_Block:
        return codegen((BlockAST*)ast, true);
    case AST_FuncProto:
        return codegen((FuncProtoAST*)ast);
    default:
        panic = true;
        return nullptr;
    }
}

llvm::Value* CodeGen::codegen(const VarExprAST *ast) {
    llvm::Value *val = symbolTable->getVar(ast->getNameId());
    if (val == nullptr) { panic = true; return nullptr; }
    return llvmBuilder.CreateLoad(val, namePool->get(ast->getNameId()));
}

llvm::Value* CodeGen::codegen(const BinExprAST *ast) {
    llvm::Value *valL;
    if (ast->getOp() == Token::O_ASGN) {
        if (ast->getL()->type() != AST_VarExpr) {
            panic = true;
            return nullptr;
        }
        const VarExprAST *var = (const VarExprAST*)ast->getL();
        valL = symbolTable->getVar(var->getNameId());
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

llvm::Value* CodeGen::codegen(const DeclAST *ast) {
    for (const auto &it : ast->getDecls()) {
        if (symbolTable->taken(it.first)) {
            panic = true;
            return nullptr;
        }

        const string &name = namePool->get(it.first);
        llvm::AllocaInst *alloca = createAlloca(name);

        const ExprAST *init = it.second.get();
        if (init != nullptr) {
            llvm::Value *initVal = codegen(init);
            if (panic || initVal == nullptr) return nullptr;

            llvmBuilder.CreateStore(initVal, alloca);
        }

        symbolTable->addVar(it.first, alloca);
    }

    return nullptr;
}

llvm::Value* CodeGen::codegen(const BlockAST *ast, bool makeScope) {
    if (makeScope) symbolTable->newScope();

    for (const auto &it : ast->getBody()) codegen(it.get());

    if (makeScope) symbolTable->endScope();

    return nullptr;
}

llvm::Value* CodeGen::codegen(const FuncProtoAST *ast) {
    // TODO funcs can share name if different args
    if (symbolTable->taken(ast->getName())) {
            panic = true;
            return nullptr;
    }

    vector<llvm::Type*> argTypes(ast->getArgs().size(), llvm::IntegerType::getInt64Ty(llvmContext));
    llvm::FunctionType *funcType = llvm::FunctionType::get(llvm::Type::getVoidTy(llvmContext), argTypes, false);
    llvm::Function *func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
            namePool->get(ast->getName()), llvmModule.get());
    
    // TODO two args can't have same names
    size_t i = 0;
    for (auto &arg : func->args()) {
        arg.setName(namePool->get(ast->getArgs()[i]));
        ++i;
    }

    symbolTable->addFunc(ast->getName(), func);

    return func;
}

void CodeGen::codegenStart() {
    llvm::FunctionType *funcTy = llvm::FunctionType::get(llvm::Type::getVoidTy(llvmContext), false);
    main = llvm::Function::Create(funcTy, llvm::Function::ExternalLinkage, "main", *llvmModule);

    llvmBuilderAlloca.SetInsertPoint(llvm::BasicBlock::Create(llvmContext, "alloca", main));
    llvmBuilder.SetInsertPoint(funcBody = llvm::BasicBlock::Create(llvmContext, "body", main));
}

void CodeGen::codegenEnd() {
    llvmBuilderAlloca.CreateBr(funcBody);
    llvmBuilder.CreateRetVoid();

    cout << endl;
    cout << "LLVM func verification:" << endl << endl;
    llvm::verifyFunction(*main, &llvm::outs());
}

void CodeGen::printout() const {
    cout << "LLVM module printout:" << endl << endl;
    // TODO sometimes prints empty function
    llvmModule->print(llvm::outs(), nullptr);
}