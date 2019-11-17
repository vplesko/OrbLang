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

llvm::Type* CodeGen::genPrimTypeI(unsigned bits) {
    return llvm::IntegerType::get(llvmContext, bits);
}

llvm::Type* CodeGen::genPrimTypeU(unsigned bits) {
    // LLVM makes no distinction between signed and unsigned int
    return llvm::IntegerType::get(llvmContext, bits);
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

void CodeGen::createCast(llvm::Value *&val, TypeTable::Id srcTypeId, llvm::Type *type, TypeTable::Id dstTypeId) {
    if (TypeTable::isTypeI(srcTypeId)) {
        val = llvmBuilder.CreateIntCast(val, symbolTable->getTypeTable()->getType(dstTypeId), true, "i_cast");
    } else if (TypeTable::isTypeU(srcTypeId)) {
        val = llvmBuilder.CreateIntCast(val, symbolTable->getTypeTable()->getType(dstTypeId), false, "u_cast");
    } else {
        panic = true;
        val = nullptr;
    }
}

void CodeGen::createCast(llvm::Value *&val, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId) {
    createCast(val, srcTypeId, symbolTable->getTypeTable()->getType(dstTypeId), dstTypeId);
}

CodeGen::ExprGenPayload CodeGen::codegenExpr(const ExprAST *ast) {
    switch (ast->type()) {
    case AST_LiteralExpr:
        return codegen((LiteralExprAST*)ast);
    case AST_VarExpr:
        return codegen((VarExprAST*)ast);
    case AST_BinExpr:
        return codegen((BinExprAST*)ast);
    case AST_CallExpr:
        return codegen((CallExprAST*)ast);
    default:
        panic = true;
        return {};
    }
}

void CodeGen::codegenNode(const BaseAST *ast, bool blockMakeScope) {
    switch (ast->type()) {
    case AST_NullExpr:
        return;
    case AST_LiteralExpr:
        codegen((LiteralExprAST*)ast);
        return;
    case AST_VarExpr:
        codegen((VarExprAST*)ast);
        return;
    case AST_BinExpr:
        codegen((BinExprAST*)ast);
        return;
    case AST_CallExpr:
        codegen((CallExprAST*)ast);
        return;
    case AST_Decl:
        codegen((DeclAST*)ast);
        return;
    case AST_If:
        codegen((IfAST*)ast);
        return;
    case AST_For:
        codegen((ForAST*) ast);
        return;
    case AST_While:
        codegen((WhileAST*) ast);
        return;
    case AST_DoWhile:
        codegen((DoWhileAST*) ast);
        return;
    case AST_Ret:
        codegen((RetAST*)ast);
        return;
    case AST_Block:
        codegen((BlockAST*)ast, blockMakeScope);
        return;
    case AST_FuncProto:
        codegen((FuncProtoAST*)ast, false);
        return;
    case AST_Func:
        codegen((FuncAST*)ast);
        return;
    default:
        panic = true;
    }
}

CodeGen::ExprGenPayload CodeGen::codegen(const LiteralExprAST *ast) {
    // TODO literals of other types
    return {ast->getType(),
        llvm::ConstantInt::get(
            symbolTable->getTypeTable()->getType(ast->getType()), 
            ast->getVal(), true)
    };
}

CodeGen::ExprGenPayload CodeGen::codegen(const VarExprAST *ast) {
    const SymbolTable::VarPayload *var = symbolTable->getVar(ast->getNameId());
    if (var == nullptr) { panic = true; return {}; }
    return {var->type, llvmBuilder.CreateLoad(var->val, namePool->get(ast->getNameId()))};
}

CodeGen::ExprGenPayload CodeGen::codegen(const BinExprAST *ast) {
    // TODO currently returning left op's type, check same type OR implicit casts
    ExprGenPayload exprPayL, exprPayR, exprPayRet;

    if (ast->getOp() == Token::O_ASGN) {
        if (ast->getL()->type() != AST_VarExpr) {
            panic = true;
            return {};
        }
        const VarExprAST *var = (const VarExprAST*)ast->getL();
        const SymbolTable::VarPayload *symVar = symbolTable->getVar(var->getNameId());
        if (symVar == nullptr) {
            panic = true;
            return {};
        }
        exprPayL.first = symVar->type;
        exprPayL.second = symVar->val;
    } else {
        exprPayL = codegenExpr(ast->getL());
    }
    if (panic || exprPayL.second == nullptr) {
        panic = true;
        return {};
    }

    exprPayR = codegenExpr(ast->getR());
    if (panic || exprPayR.second == nullptr) {
        panic = true;
        return {};
    }

    llvm::Value *valL = exprPayL.second, *valR = exprPayR.second;
    exprPayRet.first = exprPayL.first;

    if (exprPayL.first != exprPayR.first) {
        if (TypeTable::isImplicitCastable(exprPayR.first, exprPayL.first)) {
            createCast(valR, exprPayR.first, exprPayL.first);
            exprPayRet.first = exprPayL.first;
        } else if (TypeTable::isImplicitCastable(exprPayL.first, exprPayR.first) && ast->getOp() != Token::O_ASGN) {
            createCast(valL, exprPayL.first, exprPayR.first);
            exprPayRet.first = exprPayR.first;
        } else {
            panic = true;
            return {};
        }
    }

    // TODO bin ops for non-int types (float, unsigned)
    switch (ast->getOp()) {
        case Token::O_ASGN:
            llvmBuilder.CreateStore(valR, valL);
            exprPayRet.second = valR;
            break;
        case Token::O_ADD:
            exprPayRet.second = llvmBuilder.CreateAdd(valL, valR, "add_tmp");
            break;
        case Token::O_SUB:
            exprPayRet.second = llvmBuilder.CreateSub(valL, valR, "sub_tmp");
            break;
        case Token::O_MUL:
            exprPayRet.second = llvmBuilder.CreateMul(valL, valR, "mul_tmp");
            break;
        case Token::O_DIV:
            if (TypeTable::isTypeI(exprPayRet.first))
                exprPayRet.second = llvmBuilder.CreateSDiv(valL, valR, "sdiv_tmp");
            else
                exprPayRet.second = llvmBuilder.CreateUDiv(valL, valR, "udiv_tmp");
            break;
        case Token::O_EQ:
            exprPayRet.second = llvmBuilder.CreateICmpEQ(valL, valR, "cmp_eq_tmp");
            break;
        case Token::O_NEQ:
            exprPayRet.second = llvmBuilder.CreateICmpNE(valL, valR, "cmp_neq_tmp");
            break;
        case Token::O_LT:
            if (TypeTable::isTypeI(exprPayRet.first))
                exprPayRet.second = llvmBuilder.CreateICmpSLT(valL, valR, "scmp_lt_tmp");
            else
                exprPayRet.second = llvmBuilder.CreateICmpULT(valL, valR, "ucmp_lt_tmp");
            break;
        case Token::O_LTEQ:
            if (TypeTable::isTypeI(exprPayRet.first))
                exprPayRet.second = llvmBuilder.CreateICmpSLE(valL, valR, "scmp_lteq_tmp");
            else
                exprPayRet.second = llvmBuilder.CreateICmpULE(valL, valR, "ucmp_lteq_tmp");
            break;
        case Token::O_GT:
            if (TypeTable::isTypeI(exprPayRet.first))
                exprPayRet.second = llvmBuilder.CreateICmpSGT(valL, valR, "scmp_gt_tmp");
            else
                exprPayRet.second = llvmBuilder.CreateICmpUGT(valL, valR, "ucmp_gt_tmp");
            break;
        case Token::O_GTEQ:
            if (TypeTable::isTypeI(exprPayRet.first))
                exprPayRet.second = llvmBuilder.CreateICmpSGE(valL, valR, "scmp_gteq_tmp");
            else
                exprPayRet.second = llvmBuilder.CreateICmpUGE(valL, valR, "ucmp_gteq_tmp");
            break;
        default:
            panic = true;
            return {};
    }

    return exprPayRet;
}

CodeGen::ExprGenPayload CodeGen::codegen(const CallExprAST *ast) {
    FuncSignature sig;
    sig.name = ast->getName();
    sig.argTypes = vector<TypeTable::Id>(ast->getArgs().size());

    std::vector<llvm::Value*> args(ast->getArgs().size());
    for (size_t i = 0; i < ast->getArgs().size(); ++i) {
        ExprGenPayload exprPay = codegenExpr(ast->getArgs()[i].get());

        sig.argTypes[i] = exprPay.first;

        llvm::Value *arg = exprPay.second;
        if (arg == nullptr) {
            panic = true;
            return {};
        }
        args[i] = arg;
    }

    const FuncValue *func = symbolTable->getFunc(sig);
    if (func == nullptr) {
        panic = true;
        return {};
    }

    // TODO void-y type if func has no ret
    return {func->retType, llvmBuilder.CreateCall(func->func, args, "call_tmp")};
}

llvm::Type* CodeGen::codegenType(const TypeAST *ast) {
    llvm::Type *type = symbolTable->getTypeTable()->getType(ast->getTypeId());
    if (type == nullptr) {
        panic = true;
        return nullptr;
    }

    return type;
}

void CodeGen::codegen(const DeclAST *ast) {
    TypeTable::Id typeId = ast->getType()->getTypeId();
    llvm::Type *type = codegenType(ast->getType());
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
                ExprGenPayload initPay = codegenExpr(init);
                if (panic || initPay.second == nullptr) {
                    panic = true;
                    return;
                }

                llvm::Value *src = initPay.second;

                if (initPay.first != typeId) {
                    if (!TypeTable::isImplicitCastable(initPay.first, typeId)) {
                        panic = true;
                        return;
                    }

                    createCast(src, initPay.first, type, typeId);
                }

                llvmBuilder.CreateStore(src, val);
            }
        }

        symbolTable->addVar(it.first, {typeId, val});
    }
}

void CodeGen::codegen(const IfAST *ast) {
    // unlike C++, then and else may eclipse vars declared in if's init
    ScopeControl scope(ast->hasInit() ? symbolTable : nullptr);

    if (ast->hasInit()) {
        codegenNode(ast->getInit());
        if (panic) return;
    }

    // TODO check that type is bool, though LLVM is returning error anyway
    llvm::Value *condVal = codegenExpr(ast->getCond()).second;
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
        // TODO check that type is bool, though LLVM is returning error anyway
        condVal = codegenExpr(ast->getCond()).second;
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

    // TODO check that type is bool, though LLVM is returning error anyway
    llvm::Value *condVal = codegenExpr(ast->getCond()).second;
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

    // TODO check that type is bool, though LLVM is returning error anyway
    llvm::Value *condVal = codegenExpr(ast->getCond()).second;
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

    // TODO check that type is equal, implicit casts
    llvm::Value *val = codegenExpr(ast->getVal()).second;
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
    sig.argTypes = vector<TypeTable::Id>(ast->getArgCnt());
    for (size_t i = 0; i < ast->getArgCnt(); ++i) sig.argTypes[i] = ast->getArgType(i)->getTypeId();

    FuncValue *prev = symbolTable->getFunc(sig);

    if (prev != nullptr) {
        if ((prev->defined && definition) ||
            (prev->hasRet != ast->hasRetVal()) ||
            (prev->hasRet && prev->retType != ast->getRetType()->getTypeId())) {
            panic = true;
            return nullptr;
        }

        if (definition) {
            prev->defined = true;
        }

        return prev->func;
    }

    // can't have args with same name
    for (size_t i = 0; i+1 < ast->getArgCnt(); ++i) {
        for (size_t j = i+1; j < ast->getArgCnt(); ++j) {
            if (ast->getArgName(i) == ast->getArgName(j)) {
                panic = true;
                return nullptr;
            }
        }
    }

    vector<llvm::Type*> argTypes(ast->getArgCnt());
    for (size_t i = 0; i < argTypes.size(); ++i)
        argTypes[i] = symbolTable->getTypeTable()->getType(ast->getArgType(i)->getTypeId());
    llvm::Type *retType = ast->hasRetVal() ? symbolTable->getTypeTable()->getType(ast->getRetType()->getTypeId()) : llvm::Type::getVoidTy(llvmContext);
    llvm::FunctionType *funcType = llvm::FunctionType::get(retType, argTypes, false);

    llvm::Function *func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
            namePool->get(ast->getName()), llvmModule.get());
    
    size_t i = 0;
    for (auto &arg : func->args()) {
        arg.setName(namePool->get(ast->getArgName(i)));
        ++i;
    }

    FuncValue val;
    val.func = func;
    val.hasRet = ast->hasRetVal();
    if (ast->hasRetVal()) val.retType = ast->getRetType()->getTypeId();
    val.defined = definition;

    symbolTable->addFunc(sig, val);

    return func;
}

void CodeGen::codegen(const FuncAST *ast) {
    llvm::Function *func = codegen(ast->getProto(), true);
    if (panic) {
        return;
    }

    ScopeControl scope(symbolTable);

    llvmBuilderAlloca.SetInsertPoint(llvm::BasicBlock::Create(llvmContext, "alloca", func));

    llvm::BasicBlock *body = llvm::BasicBlock::Create(llvmContext, "entry", func);
    llvmBuilder.SetInsertPoint(body);

    size_t i = 0;
    for (auto &arg : func->args()) {
        const TypeAST *astArgType = ast->getProto()->getArgType(i);
        NamePool::Id astArgName = ast->getProto()->getArgName(i);
        const string &name = namePool->get(astArgName);
        llvm::AllocaInst *alloca = createAlloca(arg.getType(), name);
        llvmBuilder.CreateStore(&arg, alloca);
        symbolTable->addVar(astArgName, {astArgType->getTypeId(), alloca});

        ++i;
    }

    codegen(ast->getBody(), false);
    if (panic) {
        func->eraseFromParent();
        return;
    }

    llvmBuilderAlloca.CreateBr(body);

    if (!ast->getProto()->hasRetVal() && !isBlockTerminated())
            llvmBuilder.CreateRetVoid();

    cout << endl;
    cout << "LLVM func verification for " << namePool->get(ast->getProto()->getName()) << ":" << endl << endl;
    llvm::verifyFunction(*func, &llvm::outs());
}

void CodeGen::printout() const {
    cout << "LLVM module printout:" << endl << endl;
    llvmModule->print(llvm::outs(), nullptr);
}