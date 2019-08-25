#include "Codegen.h"
#include "Parser.h"
#include "llvm/IR/Constants.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/Verifier.h"
using namespace std;

CodeGen::CodeGen(NamePool *namePool, SymbolTable *symbolTable) : namePool(namePool), symbolTable(symbolTable), 
        llvmBuilder(llvmContext), llvmBuilderAlloca(llvmContext), panic(false) {
    llvmModule = std::make_unique<llvm::Module>(llvm::StringRef("test"), llvmContext);
}

void CodeGen::genPrimitiveTypes() {
    i64Type = namePool->add("i64");
    symbolTable->addType(i64Type, llvm::IntegerType::getInt64Ty(llvmContext));
}

llvm::Type* CodeGen::getI64Type() {
    return symbolTable->getType(i64Type);
}

llvm::AllocaInst* CodeGen::createAlloca(llvm::Type *type, const string &name) {
    return llvmBuilderAlloca.CreateAlloca(type, 0, name);
}

bool CodeGen::isBlockTerminated() const {
    return !llvmBuilder.GetInsertBlock()->empty() && llvmBuilder.GetInsertBlock()->back().isTerminator();
}

llvm::GlobalValue* CodeGen::createGlobal(llvm::Type *type, const std::string &name) {
    return new llvm::GlobalVariable(
        *llvmModule.get(),
        type,
        false,
        llvm::GlobalValue::CommonLinkage,
        nullptr,
        name);
}

llvm::Value* CodeGen::codegenNode(const BaseAST *ast, bool blockMakeScope) {
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
    case AST_For:
        codegen((ForAST*) ast);
        return nullptr;
    case AST_While:
        codegen((WhileAST*) ast);
        return nullptr;
    case AST_DoWhile:
        codegen((DoWhileAST*) ast);
        return nullptr;
    case AST_Ret:
        codegen((RetAST*)ast);
        return nullptr;
    case AST_Block:
        codegen((BlockAST*)ast, blockMakeScope);
        return nullptr;
    case AST_FuncProto:
        return codegen((FuncProtoAST*)ast, false);
    case AST_Func:
        return codegen((FuncAST*)ast);
    //case AST_Type:
    default:
        panic = true;
        return nullptr;
    }
}

llvm::Value* CodeGen::codegen(const LiteralExprAST *ast) {
    return llvm::ConstantInt::get(
        getI64Type(), 
        llvm::APInt(64, ast->getVal(), true));
}

llvm::Value* CodeGen::codegen(const VarExprAST *ast) {
    llvm::Value *val = symbolTable->getVar(ast->getNameId())->val;
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
        valL = symbolTable->getVar(var->getNameId())->val;
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
    /*
        TODO
        TODO
        TODO

        fill sig with arg types
        for this, you need to know the types
        for that, expr ast's need to know their result type

        TODO
        TODO
        TODO
    */
    FuncSignature sig;
    sig.name = ast->getName();
    sig.argTypes = vector<TypeId>(ast->getArgs().size());

    std::vector<llvm::Value*> args(ast->getArgs().size());
    for (size_t i = 0; i < ast->getArgs().size(); ++i) {
        llvm::Value *arg = codegenNode(ast->getArgs()[i].get());
        if (arg == nullptr) {
            panic = true;
            return nullptr;
        }

        args[i] = arg;
    }

    const FuncValue *func = symbolTable->getFunc(sig);
    if (func == nullptr) {
        panic = true;
        return nullptr;
    }

    return llvmBuilder.CreateCall(func->func, args, "call_tmp");
}

llvm::Type* CodeGen::codegen(const TypeAST *ast) {
    llvm::Type *type = symbolTable->getType(ast->getId());
    if (type == nullptr) {
        panic = true;
        return nullptr;
    }

    return type;
}

void CodeGen::codegen(const DeclAST *ast) {
    llvm::Type *type = codegen(ast->getType());
    if (type == nullptr) {
        panic = true;
        return;
    }

    for (const auto &it : ast->getDecls()) {
        if (symbolTable->varNameTaken(it.first)) {
            panic = true;
            return;
        }

        const string &name = namePool->get(it.first);

        llvm::Value *val;
        if (symbolTable->inGlobalScope()) {
            val = createGlobal(type, name);

            if (it.second.get() != nullptr) {
                // TODO allow init global vars
                panic = true;
                return;
            }
        } else {
            val = createAlloca(type, name);

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

        symbolTable->addVar(it.first, {ast->getType()->getId(), val});
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
    if (panic || condVal == nullptr) {
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
        codegenNode(ast->getThen(), false);
        if (panic) return;
        if (!isBlockTerminated()) llvmBuilder.CreateBr(afterBlock);
    }

    if (ast->hasElse()) {
        ScopeControl elseScope(symbolTable);
        func->getBasicBlockList().push_back(elseBlock);
        llvmBuilder.SetInsertPoint(elseBlock);
        codegenNode(ast->getElse(), false);
        if (panic) return;
        if (!isBlockTerminated()) llvmBuilder.CreateBr(afterBlock);
    }

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);
}

void CodeGen::codegen(const ForAST *ast) {
    ScopeControl scope(ast->getInit()->type() == AST_Decl ? symbolTable : nullptr);

    codegenNode(ast->getInit());
    if (panic) return;

    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    llvm::BasicBlock *condBlock = llvm::BasicBlock::Create(llvmContext, "cond", func);
    llvm::BasicBlock *bodyBlock = llvm::BasicBlock::Create(llvmContext, "body");
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(llvmContext, "after");

    llvmBuilder.CreateBr(condBlock);
    llvmBuilder.SetInsertPoint(condBlock);

    llvm::Value *condVal;
    if (ast->hasCond()) {
        condVal = codegenNode(ast->getCond());
        if (panic || condVal == nullptr) {
            panic = true;
            return;
        }
    } else {
        condVal = llvm::ConstantInt::getTrue(llvmContext);
    }

    llvmBuilder.CreateCondBr(condVal, bodyBlock, afterBlock);

    {
        ScopeControl scopeBody(symbolTable);
        func->getBasicBlockList().push_back(bodyBlock);
        llvmBuilder.SetInsertPoint(bodyBlock);

        codegenNode(ast->getBody(), false);
        if (panic) return;
    }
        
    if (ast->hasIter()) {
        codegenNode(ast->getIter());
        if (panic) return;
    }

    if (!isBlockTerminated()) llvmBuilder.CreateBr(condBlock);

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);
}

void CodeGen::codegen(const WhileAST *ast) {
    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    llvm::BasicBlock *condBlock = llvm::BasicBlock::Create(llvmContext, "cond", func);
    llvm::BasicBlock *bodyBlock = llvm::BasicBlock::Create(llvmContext, "body");
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(llvmContext, "after");

    llvmBuilder.CreateBr(condBlock);
    llvmBuilder.SetInsertPoint(condBlock);

    llvm::Value *condVal = codegenNode(ast->getCond());
    if (panic || condVal == nullptr) {
        panic = true;
        return;
    }

    llvmBuilder.CreateCondBr(condVal, bodyBlock, afterBlock);

    {
        ScopeControl scope(symbolTable);
        func->getBasicBlockList().push_back(bodyBlock);
        llvmBuilder.SetInsertPoint(bodyBlock);
        codegenNode(ast->getBody(), false);
        if (panic) return;
        if (!isBlockTerminated()) llvmBuilder.CreateBr(condBlock);
    }

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);
}

void CodeGen::codegen(const DoWhileAST *ast) {
    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    llvm::BasicBlock *bodyBlock = llvm::BasicBlock::Create(llvmContext, "body", func);
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(llvmContext, "after");

    llvmBuilder.CreateBr(bodyBlock);
    llvmBuilder.SetInsertPoint(bodyBlock);

    {
        ScopeControl scope(symbolTable);
        codegenNode(ast->getBody(), false);
        if (panic) return;
    }

    llvm::Value *condVal = codegenNode(ast->getCond());
    if (panic || condVal == nullptr) {
        panic = true;
        return;
    }

    llvmBuilder.CreateCondBr(condVal, bodyBlock, afterBlock);

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
    if (symbolTable->funcNameTaken(ast->getName())) {
        panic = true;
        return nullptr;
    }

    FuncSignature sig;
    sig.name = ast->getName();
    sig.argTypes = vector<TypeId>(ast->getArgs().size());
    for (size_t i = 0; i < ast->getArgs().size(); ++i) sig.argTypes[i] = ast->getArgs()[i].first;

    FuncValue *prev = symbolTable->getFunc(sig);

    if (prev != nullptr) {
        if ((prev->defined && definition) ||
            (prev->hasRet != ast->hasRetVal()) ||
            (prev->hasRet && prev->retType != ast->getRetType())) {
            panic = true;
            return nullptr;
        }

        if (definition) {
            prev->defined = true;
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

    vector<llvm::Type*> argTypes(ast->getArgs().size());
    for (size_t i = 0; i < argTypes.size(); ++i)
        argTypes[i] = symbolTable->getType(ast->getArgs()[i].first);
    llvm::Type *retType = ast->hasRetVal() ? symbolTable->getType(ast->getRetType()) : llvm::Type::getVoidTy(llvmContext);
    llvm::FunctionType *funcType = llvm::FunctionType::get(retType, argTypes, false);

    llvm::Function *func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
            namePool->get(ast->getName()), llvmModule.get());
    
    size_t i = 0;
    for (auto &arg : func->args()) {
        arg.setName(namePool->get(ast->getArgs()[i].second));
        ++i;
    }

    FuncValue val;
    val.func = func;
    val.hasRet = ast->hasRetVal();
    val.retType = ast->getRetType();
    val.defined = definition;

    symbolTable->addFunc(sig, val);

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
        pair<TypeId, NamePool::Id> astArg = ast->getProto()->getArgs()[i];
        const string &name = namePool->get(astArg.second);
        llvm::AllocaInst *alloca = createAlloca(arg.getType(), name);
        llvmBuilder.CreateStore(&arg, alloca);
        symbolTable->addVar(ast->getProto()->getArgs()[i].second, {astArg.first, alloca});

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