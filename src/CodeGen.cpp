#include "Codegen.h"
#include "Parser.h"
#include "llvm/IR/Constants.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/Verifier.h"
using namespace std;

CodeGen::CodeGen(const NamePool *namePool, SymbolTable *symbolTable) : namePool(namePool), symbolTable(symbolTable), 
        llvmBuilder(llvmContext), llvmBuilderAlloca(llvmContext), panic(false) {
    llvmModule = std::make_unique<llvm::Module>(llvm::StringRef("test"), llvmContext);
}

llvm::AllocaInst* CodeGen::createAlloca(const string &name) {
    return llvmBuilderAlloca.CreateAlloca(llvm::IntegerType::getInt64Ty(llvmContext), 0, name);
}

bool CodeGen::isBlockTerminated() const {
    return !llvmBuilder.GetInsertBlock()->empty() && llvmBuilder.GetInsertBlock()->back().isTerminator();
}

llvm::GlobalValue* CodeGen::createGlobal(const std::string &name) {
    return new llvm::GlobalVariable(
        *llvmModule.get(),
        llvm::IntegerType::getInt64Ty(llvmContext),
        false,
        llvm::GlobalValue::CommonLinkage,
        nullptr,
        name);
}

llvm::Value* CodeGen::codegenNode(const BaseAST *ast) {
    switch (ast->type()) {
    case AST_NullExpr:
        return nullptr;
    case AST_LiteralExpr:
        return codegen((LiteralExprAST*)ast);
    case AST_VarExpr:
        return codegen((VarExprAST*)ast);
    case AST_BinExpr:
        return codegen((BinExprAST*)ast);
    case AST_CallExpr:
        return codegen((CallExprAST*)ast);
    case AST_Decl:
        codegen((DeclAST*)ast);
        return nullptr;
    case AST_If:
        codegen((IfAST*)ast);
        return nullptr;
    case AST_Ret:
        codegen((RetAST*)ast);
        return nullptr;
    case AST_Block:
        codegen((BlockAST*)ast, true);
        return nullptr;
    case AST_FuncProto:
        return codegen((FuncProtoAST*)ast, false);
    case AST_Func:
        return codegen((FuncAST*)ast);
    default:
        panic = true;
        return nullptr;
    }
}

llvm::Value* CodeGen::codegen(const LiteralExprAST *ast) {
    return llvm::ConstantInt::get(
        llvm::IntegerType::getInt64Ty(llvmContext), 
        llvm::APInt(64, ast->getVal(), true));
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
        valL = codegenNode(ast->getL());
    }
    if (panic || valL == nullptr) {
        panic = true;
        return nullptr;
    }
    llvm::Value *valR = codegenNode(ast->getR());
    if (panic || valR == nullptr) {
        panic = true;
        return nullptr;
    }

    switch (ast->getOp()) {
        case Token::O_ASGN:
            llvmBuilder.CreateStore(valR, valL);
            return valR;
        case Token::O_ADD:
            return llvmBuilder.CreateAdd(valL, valR, "add_tmp");
        case Token::O_SUB:
            return llvmBuilder.CreateSub(valL, valR, "sub_tmp");
        case Token::O_MUL:
            return llvmBuilder.CreateMul(valL, valR, "mul_tmp");
        case Token::O_DIV:
            return llvmBuilder.CreateSDiv(valL, valR, "div_tmp");
        case Token::O_EQ:
            return llvmBuilder.CreateICmpEQ(valL, valR, "cmp_eq_tmp");
        case Token::O_NEQ:
            return llvmBuilder.CreateICmpNE(valL, valR, "cmp_neq_tmp");
        case Token::O_LT:
            return llvmBuilder.CreateICmpSLT(valL, valR, "cmp_lt_tmp");
        case Token::O_LTEQ:
            return llvmBuilder.CreateICmpSLE(valL, valR, "cmp_lteq_tmp");
        case Token::O_GT:
            return llvmBuilder.CreateICmpSGT(valL, valR, "cmp_gt_tmp");
        case Token::O_GTEQ:
            return llvmBuilder.CreateICmpSGE(valL, valR, "cmp_gteq_tmp");
        default:
            panic = true;
            return nullptr;
    }
}

llvm::Value* CodeGen::codegen(const CallExprAST *ast) {
    const FuncValue *func = symbolTable->getFunc({ast->getName(), ast->getArgs().size()});
    if (func == nullptr) {
        panic = true;
        return nullptr;
    }

    std::vector<llvm::Value*> args(ast->getArgs().size());
    for (size_t i = 0; i < ast->getArgs().size(); ++i) {
        llvm::Value *arg = codegenNode(ast->getArgs()[i].get());
        if (arg == nullptr) {
            panic = true;
            return nullptr;
        }

        args[i] = arg;
    }

    return llvmBuilder.CreateCall(func->func, args, "call_tmp");
}

void CodeGen::codegen(const DeclAST *ast) {
    for (const auto &it : ast->getDecls()) {
        if (symbolTable->taken(it.first)) {
            panic = true;
            return;
        }

        const string &name = namePool->get(it.first);

        llvm::Value *val;
        if (symbolTable->inGlobalScope()) {
            val = createGlobal(name);

            if (it.second.get() != nullptr) {
                // TODO allow init global vars
                panic = true;
                return;
            }
        } else {
            val = createAlloca(name);

            const ExprAST *init = it.second.get();
            if (init != nullptr) {
                llvm::Value *initVal = codegenNode(init);
                if (panic || initVal == nullptr) {
                    panic = true;
                    return;
                }

                llvmBuilder.CreateStore(initVal, val);
            }
        }

        symbolTable->addVar(it.first, val);
    }
}

void CodeGen::codegen(const IfAST *ast) {
    // unlike C++, then and else may eclipse vars declared in if's init
    ScopeControl scope(ast->hasInit() ? symbolTable : nullptr);

    if (ast->hasInit()) {
        codegenNode(ast->getInit());
        if (panic) return;
    }

    llvm::Value *condVal = codegenNode(ast->getCond());
    if (condVal == nullptr) {
        panic = true;
        return;
    }

    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    llvm::BasicBlock *thenBlock = llvm::BasicBlock::Create(llvmContext, "then", func);
    llvm::BasicBlock *elseBlock = ast->hasElse() ? llvm::BasicBlock::Create(llvmContext, "else") : nullptr;
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(llvmContext, "after");

    llvmBuilder.CreateCondBr(condVal, thenBlock, ast->hasElse() ? elseBlock : afterBlock);

    {
        ScopeControl thenScope(symbolTable);
        llvmBuilder.SetInsertPoint(thenBlock);
        if (ast->getThen()->type() == AST_Block) {
            codegen((BlockAST*) ast->getThen(), false);
        } else {
            codegenNode(ast->getThen());
        }
        if (panic) return;
        if (!isBlockTerminated()) llvmBuilder.CreateBr(afterBlock);
    }

    if (ast->hasElse()) {
        ScopeControl elseScope(symbolTable);
        func->getBasicBlockList().push_back(elseBlock);
        llvmBuilder.SetInsertPoint(elseBlock);
        if (ast->getElse()->type() == AST_Block) {
            codegen((BlockAST*) ast->getElse(), false);
        } else {
            codegenNode(ast->getElse());
        }
        if (panic) return;
        if (!isBlockTerminated()) llvmBuilder.CreateBr(afterBlock);
    }

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);
}

void CodeGen::codegen(const RetAST *ast) {
    if (!ast->getVal()) {
        llvmBuilder.CreateRetVoid();
        return;
    }

    llvm::Value *val = codegenNode(ast->getVal());
    if (panic || val == nullptr) {
        panic = true;
        return;
    }

    llvmBuilder.CreateRet(val);
}

void CodeGen::codegen(const BlockAST *ast, bool makeScope) {
    ScopeControl scope(makeScope ? symbolTable : nullptr);

    for (const auto &it : ast->getBody()) codegenNode(it.get());
}

llvm::Function* CodeGen::codegen(const FuncProtoAST *ast, bool definition) {
    const FuncValue *prev = symbolTable->getFunc({ast->getName(), ast->getArgs().size()});

    if (prev != nullptr) {
        if ((prev->defined && definition) ||
            (prev->hasRetVal != ast->hasRetVal())) {
            panic = true;
            return nullptr;
        }

        return prev->func;
    }

    // can't have args with same name
    for (size_t i = 0; i+1 < ast->getArgs().size(); ++i) {
        for (size_t j = i+1; j < ast->getArgs().size(); ++j) {
            if (ast->getArgs()[i] == ast->getArgs()[j]) {
                panic = true;
                return nullptr;
            }
        }
    }

    vector<llvm::Type*> argTypes(ast->getArgs().size(), llvm::IntegerType::getInt64Ty(llvmContext));
    llvm::Type *retType = ast->hasRetVal() ? llvm::IntegerType::getInt64Ty(llvmContext) : llvm::Type::getVoidTy(llvmContext);
    llvm::FunctionType *funcType = llvm::FunctionType::get(retType, argTypes, false);
    llvm::Function *func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
            namePool->get(ast->getName()), llvmModule.get());
    
    size_t i = 0;
    for (auto &arg : func->args()) {
        arg.setName(namePool->get(ast->getArgs()[i]));
        ++i;
    }

    symbolTable->addFunc(
        {ast->getName(), ast->getArgs().size()}, 
        {func, ast->hasRetVal(), definition});

    return func;
}

llvm::Function* CodeGen::codegen(const FuncAST *ast) {
    llvm::Function *func = codegen(ast->getProto(), true);
    if (panic) {
        return nullptr;
    }

    ScopeControl scope(symbolTable);

    llvmBuilderAlloca.SetInsertPoint(llvm::BasicBlock::Create(llvmContext, "alloca", func));

    llvm::BasicBlock *body = llvm::BasicBlock::Create(llvmContext, "entry", func);
    llvmBuilder.SetInsertPoint(body);

    size_t i = 0;
    for (auto &arg : func->args()) {
        const string &name = namePool->get(ast->getProto()->getArgs()[i]);
        llvm::AllocaInst *alloca = createAlloca(name);
        llvmBuilder.CreateStore(&arg, alloca);
        symbolTable->addVar(ast->getProto()->getArgs()[i], alloca);

        ++i;
    }

    codegen(ast->getBody(), false);
    if (panic) {
        func->eraseFromParent();
        return nullptr;
    }

    llvmBuilderAlloca.CreateBr(body);

    if (!ast->getProto()->hasRetVal() && !isBlockTerminated())
            llvmBuilder.CreateRetVoid();

    cout << endl;
    cout << "LLVM func verification for " << namePool->get(ast->getProto()->getName()) << ":" << endl << endl;
    llvm::verifyFunction(*func, &llvm::outs());

    return func;
}

void CodeGen::printout() const {
    cout << "LLVM module printout:" << endl << endl;
    llvmModule->print(llvm::outs(), nullptr);
}