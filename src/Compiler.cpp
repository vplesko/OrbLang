#include "Compiler.h"
#include <iostream>
#include <sstream>
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "BlockControl.h"
using namespace std;

Compiler::Compiler(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompilationMessages *msgs)
    : Processor(namePool, stringPool, typeTable, symbolTable, msgs), llvmBuilder(llvmContext), llvmBuilderAlloca(llvmContext), targetMachine(nullptr) {
    setCompiler(this);

    llvmModule = std::make_unique<llvm::Module>(llvm::StringRef("module"), llvmContext);

    llvmPmb = make_unique<llvm::PassManagerBuilder>();

    llvmFpm = make_unique<llvm::legacy::FunctionPassManager>(llvmModule.get());
    llvmPmb->populateFunctionPassManager(*llvmFpm);
}

void Compiler::printout(const std::string &filename) const {
    std::error_code errorCode;
    llvm::raw_fd_ostream dest(filename, errorCode, llvm::sys::fs::F_None);
    if (errorCode) {
        llvm::errs() << "Could not open file: " << errorCode.message();
        return;
    }

    llvmModule->print(dest, nullptr);
}

bool Compiler::binary(const std::string &filename) {
    if (targetMachine == nullptr && !initLlvmTargetMachine()) {
        return false;
    }

    std::error_code errorCode;
    llvm::raw_fd_ostream dest(filename, errorCode, llvm::sys::fs::F_None);
    if (errorCode) {
        llvm::errs() << "Could not open file: " << errorCode.message();
        return false;
    }

    llvm::legacy::PassManager llvmPm;
    llvmPmb->populateModulePassManager(llvmPm);

    llvm::CodeGenFileType fileType = llvm::CGFT_ObjectFile;

    bool failed = targetMachine->addPassesToEmitFile(llvmPm, dest, nullptr, fileType);
    if (failed) {
        llvm::errs() << "Target machine can't emit to this file type!";
        return false;
    }

    llvmPm.run(*llvmModule);
    dest.flush();

    return true;
}

void Compiler::setOptLevel(unsigned lvl) {
    llvmPmb->OptLevel = lvl;
}

llvm::Type* Compiler::genPrimTypeBool() {
    return llvm::IntegerType::get(llvmContext, 1);
}

llvm::Type* Compiler::genPrimTypeI(unsigned bits) {
    return llvm::IntegerType::get(llvmContext, bits);
}

llvm::Type* Compiler::genPrimTypeU(unsigned bits) {
    // LLVM makes no distinction between signed and unsigned int
    return llvm::IntegerType::get(llvmContext, bits);
}

llvm::Type* Compiler::genPrimTypeC(unsigned bits) {
    return llvm::IntegerType::get(llvmContext, bits);
}

llvm::Type* Compiler::genPrimTypeF32() {
    return llvm::Type::getFloatTy(llvmContext);
}

llvm::Type* Compiler::genPrimTypeF64() {
    return llvm::Type::getDoubleTy(llvmContext);
}

llvm::Type* Compiler::genPrimTypePtr() {
    return llvm::Type::getInt8PtrTy(llvmContext);
}

NodeVal Compiler::performLoad(CodeLoc codeLoc, SymbolTable::VarEntry &ref, optional<NamePool::Id> id) {
    if (!checkInLocalScope(codeLoc, true)) return NodeVal();
    if (!checkIsLlvmVal(codeLoc, ref.var, true)) return NodeVal();

    LlvmVal loadLlvmVal(ref.var.getLlvmVal().type);
    loadLlvmVal.ref = ref.var.getLlvmVal().ref;
    loadLlvmVal.lifetimeInfo = ref.var.getLlvmVal().lifetimeInfo;
    if (id.has_value()) loadLlvmVal.val = llvmBuilder.CreateLoad(ref.var.getLlvmVal().ref, getNameForLlvm(id.value()));
    else loadLlvmVal.val = llvmBuilder.CreateLoad(ref.var.getLlvmVal().ref);
    return NodeVal(codeLoc, loadLlvmVal);
}

NodeVal Compiler::performLoad(CodeLoc codeLoc, const FuncValue &func) {
    if (!checkIsLlvmFunc(codeLoc, func, true)) return NodeVal();

    LlvmVal llvmVal;
    llvmVal.type = func.getType();
    llvmVal.val = func.llvmFunc;
    return NodeVal(codeLoc, llvmVal);
}

NodeVal Compiler::performLoad(CodeLoc codeLoc, const MacroValue &macro) {
    msgs->errorInternal(codeLoc);
    return NodeVal();
}

NodeVal Compiler::performZero(CodeLoc codeLoc, TypeTable::Id ty) {
    llvm::Type *llvmType = makeLlvmTypeOrError(codeLoc, ty);
    if (llvmType == nullptr) return NodeVal();

    llvm::Constant *llvmZero = makeLlvmZero(llvmType, ty);
    if (llvmZero == nullptr) {
        msgs->errorInternal(codeLoc);
        return NodeVal();
    }

    LlvmVal llvmVal;
    llvmVal.type = ty;
    llvmVal.val = llvmZero;
    return NodeVal(codeLoc, llvmVal);
}

NodeVal Compiler::performRegister(CodeLoc codeLoc, NamePool::Id id, TypeTable::Id ty) {
    llvm::Type *llvmType = makeLlvmTypeOrError(codeLoc, ty);
    if (llvmType == nullptr) return NodeVal();

    LlvmVal llvmVal(ty);
    if (symbolTable->inGlobalScope()) {
        llvmVal.ref = makeLlvmGlobal(llvmType, nullptr, typeTable->worksAsTypeCn(ty), getNameForLlvm(id));
    } else {
        llvmVal.ref = makeLlvmAlloca(llvmType, getNameForLlvm(id));
    }
    llvmVal.lifetimeInfo.nestLevel = symbolTable->currNestLevel();

    return NodeVal(codeLoc, llvmVal);
}

NodeVal Compiler::performRegister(CodeLoc codeLoc, NamePool::Id id, const NodeVal &init) {
    NodeVal promo = promoteIfEvalValAndCheckIsLlvmVal(init, true);
    if (promo.isInvalid()) return NodeVal();

    TypeTable::Id ty = promo.getLlvmVal().type;
    llvm::Type *llvmType = makeLlvmTypeOrError(promo.getCodeLoc(), ty);
    if (llvmType == nullptr) return NodeVal();

    LlvmVal llvmVal(ty);
    if (symbolTable->inGlobalScope()) {
        llvmVal.ref = makeLlvmGlobal(llvmType, (llvm::Constant*) promo.getLlvmVal().val, typeTable->worksAsTypeCn(ty), getNameForLlvm(id));
    } else {
        llvmVal.ref = makeLlvmAlloca(llvmType, getNameForLlvm(id));
        llvmBuilder.CreateStore(promo.getLlvmVal().val, llvmVal.ref);
    }
    llvmVal.lifetimeInfo.nestLevel = symbolTable->currNestLevel();

    return NodeVal(codeLoc, llvmVal);
}

NodeVal Compiler::performCast(CodeLoc codeLoc, const NodeVal &node, TypeTable::Id ty) {
    if (!checkInLocalScope(codeLoc, true)) return NodeVal();

    NodeVal promo = promoteIfEvalValAndCheckIsLlvmVal(node, true);
    if (promo.isInvalid()) return NodeVal();

    llvm::Type *llvmType = makeLlvmTypeOrError(codeLoc, ty);
    if (llvmType == nullptr) return NodeVal();

    llvm::Value *llvmValueCast = makeLlvmCast(promo.getLlvmVal().val, promo.getLlvmVal().type, llvmType, ty);
    if (llvmValueCast == nullptr) {
        msgs->errorExprCannotCast(codeLoc, promo.getLlvmVal().type, ty);
        return NodeVal();
    }

    LlvmVal llvmVal(ty);
    llvmVal.val = llvmValueCast;
    llvmVal.lifetimeInfo.noDrop = promo.isNoDrop() || node.hasRef();
    return NodeVal(codeLoc, llvmVal);
}

bool Compiler::performBlockSetUp(CodeLoc codeLoc, SymbolTable::Block &block) {
    if (!checkInLocalScope(codeLoc, true)) return false;
    
    llvm::BasicBlock *llvmBlockBody = llvm::BasicBlock::Create(llvmContext, "body", getLlvmCurrFunction());
    llvm::BasicBlock *llvmBlockAfter = llvm::BasicBlock::Create(llvmContext, "after");

    llvmBuilder.CreateBr(llvmBlockBody);

    llvm::PHINode *llvmPhi = nullptr;
    if (block.type.has_value()) {
        llvm::Type *llvmType = makeLlvmTypeOrError(codeLoc, block.type.value());
        if (llvmType == nullptr) return false;

        llvmBuilder.SetInsertPoint(llvmBlockAfter);
        llvmPhi = llvmBuilder.CreatePHI(llvmType, 0, "block_val");
    }

    llvmBuilder.SetInsertPoint(llvmBlockBody);

    block.blockLoop = llvmBlockBody;
    block.blockExit = llvmBlockAfter;
    block.phi = llvmPhi;

    return true;
}

optional<bool> Compiler::performBlockBody(CodeLoc codeLoc, SymbolTable::Block block, const NodeVal &nodeBody) {
    if (!processChildNodes(nodeBody)) return nullopt;

    if (!isLlvmBlockTerminated()) {
        // if llvm block is terminated, these have been called already
        if (!callDropFuncsCurrBlock(codeLoc)) return nullopt;
    }

    return false;
}

// TODO if a compiled block has a jump not at the end, llvm will report it instead of this compiler (eg. on two consecutive pass instructions)
NodeVal Compiler::performBlockTearDown(CodeLoc codeLoc, SymbolTable::Block block, bool success) {
    if (!success) return NodeVal();

    if (!isLlvmBlockTerminated()) {
        if (block.type.has_value()) {
            msgs->errorBlockNoPass(codeLoc);
            return NodeVal();
        }

        llvmBuilder.CreateBr(block.blockExit);
    }

    getLlvmCurrFunction()->getBasicBlockList().push_back(block.blockExit);
    llvmBuilder.SetInsertPoint(block.blockExit);

    if (block.type.has_value()) {
        LlvmVal llvmVal(block.type.value());
        llvmVal.val = block.phi;
        return NodeVal(codeLoc, llvmVal);
    } else {
        return NodeVal(codeLoc);
    }
}

bool Compiler::performExit(CodeLoc codeLoc, SymbolTable::Block block, const NodeVal &cond) {
    return doCondBlockJump(codeLoc, cond, block.name, block.blockExit);
}

bool Compiler::performLoop(CodeLoc codeLoc, SymbolTable::Block block, const NodeVal &cond) {
    return doCondBlockJump(codeLoc, cond, block.name, block.blockLoop);
}

bool Compiler::performPass(CodeLoc codeLoc, SymbolTable::Block block, const NodeVal &val) {
    if (!checkInLocalScope(codeLoc, true)) return false;

    if (block.name.has_value()) {
        if (!callDropFuncsFromBlockToCurrBlock(codeLoc, block.name.value())) return false;
    } else {
        if (!callDropFuncsCurrBlock(codeLoc)) return false;
    }

    NodeVal valPromo = promoteIfEvalValAndCheckIsLlvmVal(val, true);
    if (valPromo.isInvalid()) return false;

    llvm::BasicBlock *llvmBlockExit = block.blockExit;
    block.phi->addIncoming(valPromo.getLlvmVal().val, llvmBuilder.GetInsertBlock());
    llvmBuilder.CreateBr(llvmBlockExit);

    return true;
}

NodeVal Compiler::performCall(CodeLoc codeLoc, const NodeVal &func, const std::vector<NodeVal> &args) {
    if (!checkInLocalScope(codeLoc, true)) return NodeVal();

    NodeVal funcPromo = promoteIfEvalValAndCheckIsLlvmVal(func, true);
    if (funcPromo.isInvalid()) return NodeVal();

    vector<llvm::Value*> llvmArgValues(args.size());
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i].isEvalVal()) {
            NodeVal llvmArg = promoteEvalVal(args[i]);
            if (llvmArg.isInvalid()) return NodeVal();
            llvmArgValues[i] = llvmArg.getLlvmVal().val;
        } else if (args[i].isLlvmVal()) {
            llvmArgValues[i] = args[i].getLlvmVal().val;
        } else {
            msgs->errorUnknown(args[i].getCodeLoc());
            return NodeVal();
        }
    }

    const TypeTable::Callable *callable = typeTable->extractCallable(funcPromo.getLlvmVal().type);
    if (callable == nullptr) {
        msgs->errorInternal(codeLoc);
        return NodeVal();
    }

    llvm::FunctionCallee llvmFuncCallee = llvm::FunctionCallee(makeLlvmFunctionType(funcPromo.getLlvmVal().type), funcPromo.getLlvmVal().val);

    if (callable->hasRet()) {
        LlvmVal retLlvmVal(callable->retType.value());
        retLlvmVal.val = llvmBuilder.CreateCall(llvmFuncCallee, llvmArgValues, "call_tmp");
        return NodeVal(codeLoc, retLlvmVal);
    } else {
        llvmBuilder.CreateCall(llvmFuncCallee, llvmArgValues, "");
        return NodeVal(codeLoc);
    }
}

NodeVal Compiler::performCall(CodeLoc codeLoc, const FuncValue &func, const std::vector<NodeVal> &args) {
    NodeVal nodeFunc = performLoad(codeLoc, func);
    if (nodeFunc.isInvalid()) return NodeVal();

    return performCall(codeLoc, nodeFunc, args);
}

NodeVal Compiler::performInvoke(CodeLoc codeLoc, const MacroValue &macro, const std::vector<NodeVal> &args) {
    msgs->errorInternal(codeLoc);
    return NodeVal();
}

bool Compiler::performFunctionDeclaration(CodeLoc codeLoc, FuncValue &func) {
    optional<string> funcLlvmName = getFuncNameForLlvm(func);
    if (!funcLlvmName.has_value()) {
        msgs->errorInternal(codeLoc);
        return false;
    }

    func.llvmFunc = llvmModule->getFunction(funcLlvmName.value());
    if (func.llvmFunc == nullptr) {
        llvm::FunctionType *llvmFuncType = makeLlvmFunctionType(func.getType());
        if (llvmFuncType == nullptr) {
            msgs->errorUnknown(codeLoc);
            return false;
        }

        func.llvmFunc = llvm::Function::Create(llvmFuncType, llvm::Function::ExternalLinkage, funcLlvmName.value(), llvmModule.get());
    }

    return true;
}

bool Compiler::performFunctionDefinition(CodeLoc codeLoc, const NodeVal &args, const NodeVal &body, FuncValue &func) {
    BlockControl blockCtrl(symbolTable, SymbolTable::CalleeValueInfo::make(func, typeTable));

    const TypeTable::Callable &callable = FuncValue::getCallable(func, typeTable);

    llvm::BasicBlock *prevLlvmBuilderAllocaInsertPoint = llvmBuilderAlloca.GetInsertBlock();
    llvmBuilderAlloca.SetInsertPoint(llvm::BasicBlock::Create(llvmContext, "alloca", func.llvmFunc));

    llvm::BasicBlock *prevLlvmBuilderInsertPoint = llvmBuilder.GetInsertBlock();
    llvm::BasicBlock *llvmBlockBody = llvm::BasicBlock::Create(llvmContext, "entry", func.llvmFunc);
    llvmBuilder.SetInsertPoint(llvmBlockBody);

    size_t i = 0;
    for (auto &llvmFuncArg : func.llvmFunc->args()) {
        llvm::Type *llvmArgType = makeLlvmTypeOrError(args.getChild(i).getCodeLoc(), callable.getArgType(i));
        if (llvmArgType == nullptr) return false;
        
        llvm::AllocaInst *llvmAlloca = makeLlvmAlloca(llvmArgType, getNameForLlvm(func.argNames[i]));
        llvmBuilder.CreateStore(&llvmFuncArg, llvmAlloca);

        LlvmVal varLlvmVal(callable.getArgType(i));
        varLlvmVal.ref = llvmAlloca;
        varLlvmVal.lifetimeInfo.noDrop = callable.getArgNoDrop(i);
        varLlvmVal.lifetimeInfo.nestLevel = symbolTable->currNestLevel();

        NodeVal varNodeVal(args.getChild(i).getCodeLoc(), varLlvmVal);

        SymbolTable::VarEntry varEntry;
        varEntry.var = move(varNodeVal);
        symbolTable->addVar(func.argNames[i], move(varEntry));

        ++i;
    }

    if (!processChildNodes(body)) {
        func.llvmFunc->eraseFromParent();
        return false;
    }

    llvmBuilderAlloca.CreateBr(llvmBlockBody);

    if (!isLlvmBlockTerminated()) {
        if (!callable.hasRet()) {
            // if llvm block is terminated, these have been called already
            if (!callDropFuncsCurrCallable(codeLoc)) return false;

            llvmBuilder.CreateRetVoid();
        } else {
            msgs->errorUnknown(codeLoc);
            return false;
        }
    }

    if (llvm::verifyFunction(*func.llvmFunc, &llvm::errs())) cerr << endl;
    llvmFpm->run(*func.llvmFunc);

    llvmBuilder.SetInsertPoint(prevLlvmBuilderInsertPoint);
    llvmBuilderAlloca.SetInsertPoint(prevLlvmBuilderAllocaInsertPoint);

    return true;
}

bool Compiler::performMacroDefinition(CodeLoc codeLoc, const NodeVal &args, const NodeVal &body, MacroValue &macro) {
    msgs->errorInternal(body.getCodeLoc());
    return false;
}

bool Compiler::performRet(CodeLoc codeLoc) {
    if (!callDropFuncsCurrCallable(codeLoc)) return false;

    // Processor already checked we are in local scope
    llvmBuilder.CreateRetVoid();

    return true;
}

bool Compiler::performRet(CodeLoc codeLoc, const NodeVal &node) {
    if (!callDropFuncsCurrCallable(codeLoc)) return false;

    NodeVal promo = promoteIfEvalValAndCheckIsLlvmVal(node, true);
    if (promo.isInvalid()) return false;

    // Processor already checked we are in local scope
    llvmBuilder.CreateRet(promo.getLlvmVal().val);

    return true;
}

NodeVal Compiler::performOperUnary(CodeLoc codeLoc, const NodeVal &oper, Oper op) {
    if (!checkInLocalScope(codeLoc, true)) return NodeVal();
    
    NodeVal promo = promoteIfEvalValAndCheckIsLlvmVal(oper, true);
    if (promo.isInvalid()) return NodeVal();

    TypeTable::Id operTy = promo.getLlvmVal().type;

    llvm::Value *llvmIn = promo.getLlvmVal().val, *llvmInRef = promo.getLlvmVal().ref;
    LlvmVal llvmVal(operTy);
    if (op == Oper::ADD) {
        if (typeTable->worksAsTypeI(operTy) ||
            typeTable->worksAsTypeU(operTy) ||
            typeTable->worksAsTypeF(operTy)) {
            llvmVal.val = llvmIn;
        }
    } else if (op == Oper::SUB) {
        if (typeTable->worksAsTypeI(operTy)) {
            llvmVal.val = llvmBuilder.CreateNeg(llvmIn, "sneg_tmp");
        } else if (typeTable->worksAsTypeF(operTy)) {
            llvmVal.val = llvmBuilder.CreateFNeg(llvmIn, "fneg_tmp");
        }
    } else if (op == Oper::BIT_NOT) {
        if (typeTable->worksAsTypeI(operTy) ||
            typeTable->worksAsTypeU(operTy)) {
            llvmVal.val = llvmBuilder.CreateNot(llvmIn, "bit_not_tmp");
        }
    } else if (op == Oper::NOT) {
        if (typeTable->worksAsTypeB(operTy)) {
            llvmVal.val = llvmBuilder.CreateNot(llvmIn, "not_tmp");
        }
    } else if (op == Oper::BIT_AND) {
        if (llvmInRef != nullptr) {
            llvmVal.type = typeTable->addTypeAddrOf(operTy);
            llvmVal.val = llvmInRef;
        }
    }

    if (llvmVal.val == nullptr) {
        msgs->errorUnknown(codeLoc);
        return NodeVal();
    }

    return NodeVal(codeLoc, llvmVal);
}

NodeVal Compiler::performOperUnaryDeref(CodeLoc codeLoc, const NodeVal &oper, TypeTable::Id resTy) {
    if (!checkInLocalScope(codeLoc, true)) return NodeVal();
    if (!checkIsLlvmVal(oper, true)) return NodeVal();

    llvm::Value *llvmIn = oper.getLlvmVal().val;

    LlvmVal llvmVal(resTy);
    llvmVal.val = llvmBuilder.CreateLoad(llvmIn, "deref_tmp");
    llvmVal.ref = llvmIn;
    return NodeVal(codeLoc, llvmVal);
}

ComparisonSignal Compiler::performOperComparisonSetUp(CodeLoc codeLoc, size_t opersCnt) {
    ComparisonSignal compSignal;

    llvm::BasicBlock *llvmBlockCurr = llvmBuilder.GetInsertBlock();

    compSignal.llvmBlock = llvm::BasicBlock::Create(llvmContext, "comparison");
    llvmBuilder.SetInsertPoint(compSignal.llvmBlock);
    compSignal.llvmPhi = llvmBuilder.CreatePHI(makeLlvmPrimType(TypeTable::P_BOOL), opersCnt);

    llvmBuilder.SetInsertPoint(llvmBlockCurr);

    return compSignal;
}

optional<bool> Compiler::performOperComparison(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op, ComparisonSignal &signal) {
    if (!checkInLocalScope(codeLoc, true)) return nullopt;

    NodeVal lhsPromo = promoteIfEvalValAndCheckIsLlvmVal(lhs, true);
    if (lhsPromo.isInvalid()) return nullopt;

    NodeVal rhsPromo = promoteIfEvalValAndCheckIsLlvmVal(rhs, true);
    if (rhsPromo.isInvalid()) return nullopt;

    bool isTypeI = typeTable->worksAsTypeI(lhsPromo.getType().value());
    bool isTypeU = typeTable->worksAsTypeU(lhsPromo.getType().value());
    bool isTypeC = typeTable->worksAsTypeC(lhsPromo.getType().value());
    bool isTypeF = typeTable->worksAsTypeF(lhsPromo.getType().value());
    bool isTypeP = typeTable->worksAsTypeAnyP(lhsPromo.getType().value());
    bool isTypeB = typeTable->worksAsTypeB(lhsPromo.getType().value());
    bool isTypeCall = typeTable->worksAsCallable(lhsPromo.getType().value());

    llvm::Value *llvmValueRes = nullptr;

    switch (op) {
    case Oper::EQ:
        if (isTypeI || isTypeU || isTypeC || isTypeB) {
            llvmValueRes = llvmBuilder.CreateICmpEQ(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "cmp_eq_tmp");
        } else if (isTypeF) {
            llvmValueRes = llvmBuilder.CreateFCmpOEQ(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "fcmp_eq_tmp");
        } else if (isTypeP || isTypeCall) {
            llvmValueRes = llvmBuilder.CreateICmpEQ(
                llvmBuilder.CreatePtrToInt(lhsPromo.getLlvmVal().val, makeLlvmPrimType(TypeTable::WIDEST_I)),
                llvmBuilder.CreatePtrToInt(rhsPromo.getLlvmVal().val, makeLlvmPrimType(TypeTable::WIDEST_I)),
                "pcmp_eq_tmp");
        }
        break;
    case Oper::NE:
        if (isTypeI || isTypeU || isTypeC || isTypeB) {
            llvmValueRes = llvmBuilder.CreateICmpNE(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "cmp_ne_tmp");
        } else if (isTypeF) {
            llvmValueRes = llvmBuilder.CreateFCmpONE(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "fcmp_ne_tmp");
        } else if (isTypeP || isTypeCall) {
            llvmValueRes = llvmBuilder.CreateICmpNE(
                llvmBuilder.CreatePtrToInt(lhsPromo.getLlvmVal().val, makeLlvmPrimType(TypeTable::WIDEST_I)),
                llvmBuilder.CreatePtrToInt(rhsPromo.getLlvmVal().val, makeLlvmPrimType(TypeTable::WIDEST_I)),
                "pcmp_ne_tmp");
        }
        break;
    case Oper::LT:
        if (isTypeI) {
            llvmValueRes = llvmBuilder.CreateICmpSLT(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "scmp_lt_tmp");
        } else if (isTypeU || isTypeC) {
            llvmValueRes = llvmBuilder.CreateICmpULT(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "ucmp_lt_tmp");
        } else if (isTypeF) {
            llvmValueRes = llvmBuilder.CreateFCmpOLT(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "fcmp_lt_tmp");
        }
        break;
    case Oper::LE:
        if (isTypeI) {
            llvmValueRes = llvmBuilder.CreateICmpSLE(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "scmp_le_tmp");
        } else if (isTypeU || isTypeC) {
            llvmValueRes = llvmBuilder.CreateICmpULE(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "ucmp_le_tmp");
        } else if (isTypeF) {
            llvmValueRes = llvmBuilder.CreateFCmpOLE(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "fcmp_le_tmp");
        }
        break;
    case Oper::GT:
        if (isTypeI) {
            llvmValueRes = llvmBuilder.CreateICmpSGT(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "scmp_gt_tmp");
        } else if (isTypeU || isTypeC) {
            llvmValueRes = llvmBuilder.CreateICmpUGT(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "ucmp_gt_tmp");
        } else if (isTypeF) {
            llvmValueRes = llvmBuilder.CreateFCmpOGT(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "fcmp_gt_tmp");
        }
        break;
    case Oper::GE:
        if (isTypeI) {
            llvmValueRes = llvmBuilder.CreateICmpSGE(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "scmp_ge_tmp");
        } else if (isTypeU || isTypeC) {
            llvmValueRes = llvmBuilder.CreateICmpUGE(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "ucmp_ge_tmp");
        } else if (isTypeF) {
            llvmValueRes = llvmBuilder.CreateFCmpOGE(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "fcmp_ge_tmp");
        }
        break;
    default:
        break;
    }

    if (llvmValueRes == nullptr) {
        msgs->errorUnknown(rhs.getCodeLoc());
        return nullopt;
    }

    signal.llvmPhi->addIncoming(getLlvmConstB(false), llvmBuilder.GetInsertBlock());

    llvm::BasicBlock *llvmBlockNext = llvm::BasicBlock::Create(llvmContext, "next", getLlvmCurrFunction());
    llvmBuilder.CreateCondBr(llvmValueRes, llvmBlockNext, signal.llvmBlock);

    llvmBuilder.SetInsertPoint(llvmBlockNext);

    return false;
}

NodeVal Compiler::performOperComparisonTearDown(CodeLoc codeLoc, bool success, ComparisonSignal signal) {
    if (!checkInLocalScope(codeLoc, true)) return NodeVal();

    if (!success) {
        signal.llvmPhi->eraseFromParent();
        return NodeVal();
    }

    signal.llvmPhi->addIncoming(getLlvmConstB(true), llvmBuilder.GetInsertBlock());
    llvmBuilder.CreateBr(signal.llvmBlock);

    getLlvmCurrFunction()->getBasicBlockList().push_back(signal.llvmBlock);
    llvmBuilder.SetInsertPoint(signal.llvmBlock);

    LlvmVal llvmVal(typeTable->getPrimTypeId(TypeTable::P_BOOL));
    llvmVal.val = signal.llvmPhi;
    return NodeVal(codeLoc, llvmVal);
}

NodeVal Compiler::performOperAssignment(CodeLoc codeLoc, NodeVal &lhs, const NodeVal &rhs) {
    if (!checkInLocalScope(codeLoc, true)) return NodeVal();

    if (!checkIsLlvmVal(lhs, true)) return NodeVal();
    
    NodeVal rhsPromo = promoteIfEvalValAndCheckIsLlvmVal(rhs, true);
    if (rhsPromo.isInvalid()) return NodeVal();

    llvmBuilder.CreateStore(rhsPromo.getLlvmVal().val, lhs.getLlvmVal().ref);

    LlvmVal llvmVal(lhs.getType().value());
    llvmVal.val = rhsPromo.getLlvmVal().val;
    llvmVal.ref = lhs.getLlvmVal().ref;
    llvmVal.lifetimeInfo = lhs.getLlvmVal().lifetimeInfo;
    return NodeVal(lhs.getCodeLoc(), llvmVal);
}

NodeVal Compiler::performOperIndex(CodeLoc codeLoc, NodeVal &base, const NodeVal &ind, TypeTable::Id resTy) {
    if (!checkInLocalScope(codeLoc, true)) return NodeVal();
    
    NodeVal basePromo = promoteIfEvalValAndCheckIsLlvmVal(base, true);
    if (basePromo.isInvalid()) return NodeVal();
    
    NodeVal indPromo = promoteIfEvalValAndCheckIsLlvmVal(ind, true);
    if (ind.isInvalid()) return NodeVal();

    LlvmVal llvmVal(resTy);
    if (typeTable->worksAsTypeArrP(basePromo.getType().value())) {
        llvmVal.ref = llvmBuilder.CreateGEP(basePromo.getLlvmVal().val, indPromo.getLlvmVal().val);
        llvmVal.val = llvmBuilder.CreateLoad(llvmVal.ref, "index_tmp");
    } else if (typeTable->worksAsTypeArr(basePromo.getType().value())) {
        llvm::Type *llvmTypeInd = makeLlvmTypeOrError(indPromo.getCodeLoc(), indPromo.getType().value());
        if (llvmTypeInd == nullptr) return NodeVal();

        if (basePromo.hasRef()) {
            llvmVal.ref = llvmBuilder.CreateGEP(basePromo.getLlvmVal().ref,
                {llvm::ConstantInt::get(llvmTypeInd, 0), indPromo.getLlvmVal().val});
            llvmVal.val = llvmBuilder.CreateLoad(llvmVal.ref, "index_tmp");
        } else {
            llvm::Type *llvmTypeBase = makeLlvmTypeOrError(basePromo.getCodeLoc(), basePromo.getType().value());
            if (llvmTypeBase == nullptr) return NodeVal();

            // llvm's extractvalue would require compile-time constant indices
            llvm::Value *tmp = makeLlvmAlloca(llvmTypeBase, "tmp");
            llvmBuilder.CreateStore(basePromo.getLlvmVal().val, tmp);
            tmp = llvmBuilder.CreateGEP(tmp,
                {llvm::ConstantInt::get(llvmTypeInd, 0), indPromo.getLlvmVal().val});
            llvmVal.val = llvmBuilder.CreateLoad(tmp, "index_tmp");
        }

        llvmVal.lifetimeInfo = basePromo.getLlvmVal().lifetimeInfo;
    } else {
        msgs->errorInternal(codeLoc);
        return NodeVal();
    }
    return NodeVal(basePromo.getCodeLoc(), llvmVal);
}

NodeVal Compiler::performOperDot(CodeLoc codeLoc, NodeVal &base, std::uint64_t ind, TypeTable::Id resTy) {
    if (!checkInLocalScope(codeLoc, true)) return NodeVal();
    
    NodeVal basePromo = promoteIfEvalValAndCheckIsLlvmVal(base, true);
    if (basePromo.isInvalid()) return NodeVal();

    LlvmVal llvmVal(resTy);
    if (basePromo.hasRef()) {
        llvmVal.ref = llvmBuilder.CreateStructGEP(basePromo.getLlvmVal().ref, (unsigned) ind);
        llvmVal.val = llvmBuilder.CreateLoad(llvmVal.ref, "dot_tmp");
    } else {
        llvmVal.val = llvmBuilder.CreateExtractValue(basePromo.getLlvmVal().val, {(unsigned) ind}, "dot_tmp");
    }
    llvmVal.lifetimeInfo = basePromo.getLlvmVal().lifetimeInfo;
    return NodeVal(basePromo.getCodeLoc(), llvmVal);
}

NodeVal Compiler::performOperRegular(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op, bool bare) {
    if (!checkInLocalScope(codeLoc, true)) return NodeVal();
    
    NodeVal lhsPromo = promoteIfEvalValAndCheckIsLlvmVal(lhs, true);
    if (lhs.isInvalid()) return NodeVal();
    
    NodeVal rhsPromo = promoteIfEvalValAndCheckIsLlvmVal(rhs, true);
    if (rhs.isInvalid()) return NodeVal();

    LlvmVal llvmVal(lhs.getType().value());
    
    bool isTypeI = typeTable->worksAsTypeI(llvmVal.type);
    bool isTypeU = typeTable->worksAsTypeU(llvmVal.type);
    bool isTypeF = typeTable->worksAsTypeF(llvmVal.type);
    
    switch (op) {
    case Oper::ADD:
        if (isTypeI || isTypeU) {
            llvmVal.val = llvmBuilder.CreateAdd(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "add_tmp");
        } else if (isTypeF) {
            llvmVal.val = llvmBuilder.CreateFAdd(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "fadd_tmp");
        }
        break;
    case Oper::SUB:
        if (isTypeI || isTypeU) {
            llvmVal.val = llvmBuilder.CreateSub(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "sub_tmp");
        } else if (isTypeF) {
            llvmVal.val = llvmBuilder.CreateFSub(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "fsub_tmp");
        }
        break;
    case Oper::MUL:
        if (isTypeI || isTypeU) {
            llvmVal.val = llvmBuilder.CreateMul(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "mul_tmp");
        } else if (isTypeF) {
            llvmVal.val = llvmBuilder.CreateFMul(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "fmul_tmp");
        }
        break;
    case Oper::DIV:
        if (isTypeI) {
            llvmVal.val = llvmBuilder.CreateSDiv(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "sdiv_tmp");
        } else if (isTypeU) {
            llvmVal.val = llvmBuilder.CreateUDiv(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "udiv_tmp");
        } else if (isTypeF) {
            llvmVal.val = llvmBuilder.CreateFDiv(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "fdiv_tmp");
        }
        break;
    case Oper::REM:
        if (isTypeI) {
            llvmVal.val = llvmBuilder.CreateSRem(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "srem_tmp");
        } else if (isTypeU) {
            llvmVal.val = llvmBuilder.CreateURem(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "urem_tmp");
        } else if (isTypeF) {
            llvmVal.val = llvmBuilder.CreateFRem(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "frem_tmp");
        }
        break;
    case Oper::SHL:
        if (isTypeI || isTypeU) {
            llvmVal.val = llvmBuilder.CreateShl(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "shl_tmp");
        }
        break;
    case Oper::SHR:
        if (isTypeI) {
            llvmVal.val = llvmBuilder.CreateAShr(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "ashr_tmp");
        } else if (isTypeU) {
            llvmVal.val = llvmBuilder.CreateLShr(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "lshr_tmp");
        }
        break;
    case Oper::BIT_AND:
        if (isTypeI || isTypeU) {
            llvmVal.val = llvmBuilder.CreateAnd(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "and_tmp");
        }
        break;
    case Oper::BIT_OR:
        if (isTypeI || isTypeU) {
            llvmVal.val = llvmBuilder.CreateOr(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "or_tmp");
        }
        break;
    case Oper::BIT_XOR:
        if (isTypeI || isTypeU) {
            llvmVal.val = llvmBuilder.CreateXor(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "xor_tmp");
        }
        break;
    default:
        break;
    }
    
    if (llvmVal.val == nullptr) {
        msgs->errorUnknown(rhs.getCodeLoc());
        return NodeVal();
    }

    return NodeVal(rhs.getCodeLoc(), llvmVal);
}

optional<uint64_t> Compiler::performSizeOf(CodeLoc codeLoc, TypeTable::Id ty) {
    if (targetMachine == nullptr && !initLlvmTargetMachine()) {
        msgs->errorInternal(codeLoc);
        return nullopt;
    }

    llvm::Type *llvmType = makeLlvmTypeOrError(codeLoc, ty);
    if (llvmType == nullptr) return nullopt;

    return targetMachine->createDataLayout().getTypeAllocSize(llvmType).getFixedSize();
}

bool Compiler::doCondBlockJump(CodeLoc codeLoc, const NodeVal &cond, optional<NamePool::Id> blockName, llvm::BasicBlock *llvmBlock) {
    if (!checkInLocalScope(codeLoc, true)) return false;

    NodeVal condPromo = promoteIfEvalValAndCheckIsLlvmVal(cond, true);
    if (condPromo.isInvalid()) return false;

    llvm::BasicBlock *llvmBlockDrops = llvm::BasicBlock::Create(llvmContext, "drops");
    llvm::BasicBlock *llvmBlockAfter = llvm::BasicBlock::Create(llvmContext, "after");

    llvmBuilder.CreateCondBr(condPromo.getLlvmVal().val, llvmBlockDrops, llvmBlockAfter);

    getLlvmCurrFunction()->getBasicBlockList().push_back(llvmBlockDrops);
    llvmBuilder.SetInsertPoint(llvmBlockDrops);

    if (blockName.has_value()) {
        if (!callDropFuncsFromBlockToCurrBlock(codeLoc, blockName.value())) return false;
    } else {
        if (!callDropFuncsCurrBlock(codeLoc)) return false;
    }

    llvmBuilder.CreateBr(llvmBlock);

    getLlvmCurrFunction()->getBasicBlockList().push_back(llvmBlockAfter);
    llvmBuilder.SetInsertPoint(llvmBlockAfter);

    return true;
}

NodeVal Compiler::promoteEvalVal(CodeLoc codeLoc, const EvalVal &eval) {
    TypeTable::Id ty = eval.type;

    llvm::Constant *llvmConst = nullptr;
    if (EvalVal::isI(eval, typeTable)) {
        llvmConst = llvm::ConstantInt::get(makeLlvmType(ty), EvalVal::getValueI(eval, typeTable).value(), true);
    } else if (EvalVal::isU(eval, typeTable)) {
        llvmConst = llvm::ConstantInt::get(makeLlvmType(ty), EvalVal::getValueU(eval, typeTable).value(), false);
    } else if (EvalVal::isF(eval, typeTable)) {
        llvmConst = llvm::ConstantFP::get(makeLlvmType(ty), EvalVal::getValueF(eval, typeTable).value());
    } else if (EvalVal::isC(eval, typeTable)) {
        llvmConst = llvm::ConstantInt::get(makeLlvmType(ty), (uint8_t) eval.c8, false);
    } else if (EvalVal::isB(eval, typeTable)) {
        llvmConst = getLlvmConstB(eval.b);
    } else if (EvalVal::isNull(eval, typeTable)) {
        llvmConst = llvm::ConstantPointerNull::get((llvm::PointerType*) makeLlvmType(ty));
    } else if (EvalVal::isNonNullStr(eval, typeTable)) {
        llvmConst = stringPool->getLlvm(eval.str.value());
        if (llvmConst == nullptr) {
            llvmConst = makeLlvmConstString(stringPool->get(eval.str.value()));
            stringPool->setLlvm(eval.str.value(), llvmConst);
        }
    } else if (EvalVal::isArr(eval, typeTable)) {
        llvm::ArrayType *llvmArrayType = (llvm::ArrayType*) makeLlvmTypeOrError(codeLoc, eval.type);
        if (llvmArrayType == nullptr) return NodeVal();

        vector<llvm::Constant*> llvmConsts;
        llvmConsts.reserve(eval.elems.size());
        for (const NodeVal &elem : eval.elems) {
            NodeVal elemPromo = promoteEvalVal(codeLoc, elem.getEvalVal());
            if (elemPromo.isInvalid()) return NodeVal();
            llvmConsts.push_back((llvm::Constant*) elemPromo.getLlvmVal().val);
        }

        llvmConst = llvm::ConstantArray::get(llvmArrayType, llvmConsts);
    } else if (EvalVal::isTuple(eval, typeTable)) {
        vector<llvm::Constant*> llvmConsts;
        llvmConsts.reserve(eval.elems.size());
        for (const NodeVal &elem : eval.elems) {
            NodeVal elemPromo = promoteEvalVal(codeLoc, elem.getEvalVal());
            if (elemPromo.isInvalid()) return NodeVal();
            llvmConsts.push_back((llvm::Constant*) elemPromo.getLlvmVal().val);
        }

        llvmConst = llvm::ConstantStruct::getAnon(llvmContext, llvmConsts);
    } else if (EvalVal::isDataType(eval, typeTable)) {
        llvm::StructType *llvmStructType = (llvm::StructType*) makeLlvmTypeOrError(codeLoc, eval.type);
        if (llvmStructType == nullptr) return NodeVal();

        vector<llvm::Constant*> llvmConsts;
        llvmConsts.reserve(eval.elems.size());
        for (const NodeVal &elem : eval.elems) {
            NodeVal elemPromo = promoteEvalVal(codeLoc, elem.getEvalVal());
            if (elemPromo.isInvalid()) return NodeVal();
            llvmConsts.push_back((llvm::Constant*) elemPromo.getLlvmVal().val);
        }

        llvmConst = llvm::ConstantStruct::get(llvmStructType, llvmConsts);
    } else if (EvalVal::isFunc(eval, typeTable)) {
        const FuncValue *funcVal = EvalVal::getValueFunc(eval, typeTable).value();
        if (funcVal == nullptr) {
            llvmConst = llvm::ConstantPointerNull::get((llvm::PointerType*) makeLlvmType(ty));
        } else {
            if (funcVal->isLlvm()) llvmConst = funcVal->llvmFunc;
        }
    }

    if (llvmConst == nullptr) {
        msgs->errorExprCannotPromote(codeLoc);
        return NodeVal();
    }

    LlvmVal llvmVal(ty);
    llvmVal.val = llvmConst;
    llvmVal.lifetimeInfo.noDrop = eval.lifetimeInfo.noDrop;

    return NodeVal(codeLoc, llvmVal);
}

NodeVal Compiler::promoteEvalVal(const NodeVal &node) {
    return promoteEvalVal(node.getCodeLoc(), node.getEvalVal());
}

NodeVal Compiler::promoteIfEvalValAndCheckIsLlvmVal(const NodeVal &node, bool orError) {
    NodeVal promo;
    if (node.isEvalVal()) promo = promoteEvalVal(node);
    else promo = node;

    if (promo.isInvalid()) return NodeVal();
    if (!checkIsLlvmVal(promo, orError)) return NodeVal();
    return promo;
}

string Compiler::getNameForLlvm(NamePool::Id name) const {
    // LLVM is smart enough to put quotes around IDs with special chars, but let's keep this method in anyway.
    return namePool->get(name);
}

optional<string> Compiler::getFuncNameForLlvm(const FuncValue &func) {
    optional<string> sigTyStr = typeTable->makeBinString(func.getType(), namePool, true);
    if (!sigTyStr.has_value()) return nullopt;

    stringstream ss;
    ss << getNameForLlvm(func.name);
    if (!func.noNameMangle) ss << sigTyStr.value();
    return ss.str();
}

bool Compiler::isLlvmBlockTerminated() const {
    return !llvmBuilder.GetInsertBlock()->empty() && llvmBuilder.GetInsertBlock()->back().isTerminator();
}

llvm::Constant* Compiler::getLlvmConstB(bool val) {
    if (val) return llvm::ConstantInt::getTrue(llvmContext);
    else return llvm::ConstantInt::getFalse(llvmContext);
}

llvm::Constant* Compiler::makeLlvmConstString(const std::string &str) {
    return llvmBuilder.CreateGlobalStringPtr(str, "str_lit", 0, llvmModule.get());
}

llvm::FunctionType* Compiler::makeLlvmFunctionType(TypeTable::Id typeId) {
    const TypeTable::Callable *callable = typeTable->extractCallable(typeId);
    if (callable == nullptr || !callable->isFunc) return nullptr;

    vector<llvm::Type*> llvmArgTypes(callable->getArgCnt());
    for (size_t i = 0; i < callable->getArgCnt(); ++i) {
        llvmArgTypes[i] = makeLlvmType(callable->getArgType(i));
        if (llvmArgTypes[i] == nullptr) return nullptr;
    }
    
    llvm::Type *llvmRetType = callable->retType.has_value() ? makeLlvmType(callable->retType.value()) : llvm::Type::getVoidTy(llvmContext);
    if (llvmRetType == nullptr) return nullptr;

    return llvm::FunctionType::get(llvmRetType, llvmArgTypes, callable->variadic);
}

llvm::Type* Compiler::makeLlvmType(TypeTable::Id typeId) {
    llvm::Type *llvmType = typeTable->getType(typeId);
    if (llvmType != nullptr) {
        if (typeTable->isDataType(typeId)) {
            bool definitionCompiled = !((llvm::StructType*) llvmType)->isOpaque();
            bool definitionProcessed = typeTable->getDataType(typeId).defined;
            if (definitionCompiled == definitionProcessed) return llvmType;
        } else {
            return llvmType;
        }
    }

    if (typeTable->isTypeDescr(typeId)) {
        const TypeTable::TypeDescr &descr = typeTable->getTypeDescr(typeId);

        llvmType = makeLlvmType(descr.base);
        if (llvmType == nullptr) return nullptr;

        for (const TypeTable::TypeDescr::Decor &decor : descr.decors) {
            switch (decor.type) {
            case TypeTable::TypeDescr::Decor::D_PTR:
            case TypeTable::TypeDescr::Decor::D_ARR_PTR:
                llvmType = llvm::PointerType::get(llvmType, 0);
                break;
            case TypeTable::TypeDescr::Decor::D_ARR:
                llvmType = llvm::ArrayType::get(llvmType, decor.len);
                break;
            default:
                return nullptr;
            }
        }
    } else if (typeTable->isTuple(typeId)) {
        const TypeTable::Tuple &tup = typeTable->getTuple(typeId);

        vector<llvm::Type*> memberTypes(tup.members.size());
        for (size_t i = 0; i < tup.members.size(); ++i) {
            llvm::Type *memberType = makeLlvmType(tup.members[i]);
            if (memberType == nullptr) return nullptr;
            memberTypes[i] = memberType;
        }

        llvmType = llvm::StructType::get(llvmContext, memberTypes);
    } else if (typeTable->isFixedType(typeId)) {
        llvmType = makeLlvmType(typeTable->getFixedType(typeId).type);
        if (llvmType == nullptr) return nullptr;
    } else if (typeTable->isDataType(typeId)) {
        const TypeTable::DataType &data = typeTable->getDataType(typeId);

        // pre-declare
        if (llvmType == nullptr) {
            llvmType = llvm::StructType::create(llvmContext, namePool->get(data.name));
            typeTable->setType(typeId, llvmType);
        }

        if (data.defined) {
            // set to dummy body to not be opaque, to avoid potential infinite recursion (llvm doesn't allow empty bodies)
            llvm::Type *dummy = makeLlvmType(typeTable->getPrimTypeId(TypeTable::P_BOOL));
            ((llvm::StructType*) llvmType)->setBody(dummy);

            vector<llvm::Type*> memberTypes(data.members.size());
            for (size_t i = 0; i < data.members.size(); ++i) {
                llvm::Type *memberType = makeLlvmType(data.members[i].type);
                if (memberType == nullptr) return nullptr;
                memberTypes[i] = memberType;
            }
            ((llvm::StructType*) llvmType)->setBody(memberTypes);
        }
    } else if (typeTable->isCallable(typeId)) {
        llvmType = makeLlvmFunctionType(typeId);
        if (llvmType == nullptr) return nullptr;
        // make a function pointer type
        llvmType = llvm::PointerType::get(llvmType, 0);
    }

    // supported primitive type are compiled at the start of compilation
    // in case of id or type, nullptr is returned    
    typeTable->setType(typeId, llvmType);
    return llvmType;
}

llvm::Type* Compiler::makeLlvmTypeOrError(CodeLoc codeLoc, TypeTable::Id typeId) {
    llvm::Type *ret = makeLlvmType(typeId);
    if (ret == nullptr) {
        msgs->errorTypeCannotCompile(codeLoc, typeId);
    }
    return ret;
}

llvm::Constant* Compiler::makeLlvmZero(TypeTable::Id typeId) {
    return makeLlvmZero(makeLlvmType(typeId), typeId);
}

llvm::Constant* Compiler::makeLlvmZero(llvm::Type *llvmType, TypeTable::Id typeId) {
    llvm::Constant *llvmZero;
    if (typeTable->worksAsTypeArr(typeId)) {
        TypeTable::Id elemTypeId = typeTable->addTypeIndexOf(typeId).value();
        llvm::Constant *elemLlvmZero = makeLlvmZero(elemTypeId);
        vector<llvm::Constant*> elemVals(typeTable->extractLenOfArr(typeId).value(), elemLlvmZero);

        llvmZero = llvm::ConstantArray::get((llvm::ArrayType*) llvmType, elemVals);
    } else if (typeTable->worksAsTuple(typeId)) {
        const TypeTable::Tuple &tuple = *typeTable->extractTuple(typeId);

        vector<llvm::Constant*> membVals;
        membVals.reserve(tuple.members.size());
        for (TypeTable::Id memb : tuple.members) {
            llvm::Constant *membLlvmZero = makeLlvmZero(memb);
            membVals.push_back(membLlvmZero);
        }

        llvmZero = llvm::ConstantStruct::get((llvm::StructType*) llvmType, membVals);
    } else if (typeTable->worksAsDataType(typeId)) {
        const TypeTable::DataType &dataType = *typeTable->extractDataType(typeId);

        vector<llvm::Constant*> membVals;
        membVals.reserve(dataType.members.size());
        for (const TypeTable::DataType::MembEntry &memb : dataType.members) {
            llvm::Type *membLlvmType = makeLlvmType(memb.type);
            llvm::Constant *membLlvmZero = memb.noZeroInit ? llvm::UndefValue::get(membLlvmType) : makeLlvmZero(membLlvmType, memb.type);
            membVals.push_back(membLlvmZero);
        }

        llvmZero = llvm::ConstantStruct::get((llvm::StructType*) llvmType, membVals);
    } else {
        llvmZero = llvm::Constant::getNullValue(llvmType);
    }

    return llvmZero;
}

llvm::GlobalValue* Compiler::makeLlvmGlobal(llvm::Type *type, llvm::Constant *init, bool isConstant, const std::string &name) {
    if (init == nullptr) init = llvm::Constant::getNullValue(type);

    return new llvm::GlobalVariable(
        *llvmModule,
        type,
        isConstant,
        // TODO are linkage values correct (this and other(s))
        llvm::GlobalValue::PrivateLinkage,
        init,
        name);
}

llvm::AllocaInst* Compiler::makeLlvmAlloca(llvm::Type *type, const std::string &name) {
    return llvmBuilderAlloca.CreateAlloca(type, nullptr, name);
}

llvm::Value* Compiler::makeLlvmCast(llvm::Value *srcLlvmVal, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId) {
    llvm::Type *dstLlvmType = makeLlvmType(dstTypeId);
    if (dstLlvmType == nullptr) return nullptr;

    return makeLlvmCast(srcLlvmVal, srcTypeId, dstLlvmType, dstTypeId);
}

llvm::Value* Compiler::makeLlvmCast(llvm::Value *srcLlvmVal, TypeTable::Id srcTypeId, llvm::Type *dstLlvmType, TypeTable::Id dstTypeId) {
    // TODO early catch case when just changing constness
    if (srcTypeId == dstTypeId) return srcLlvmVal;

    llvm::Value *dstLlvmVal = nullptr;

    if (typeTable->worksAsTypeI(srcTypeId)) {
        if (typeTable->worksAsTypeI(dstTypeId)) {
            dstLlvmVal = llvmBuilder.CreateIntCast(srcLlvmVal, dstLlvmType, true, "i2i_cast");
        } else if (typeTable->worksAsTypeU(dstTypeId)) {
            dstLlvmVal = llvmBuilder.CreateIntCast(srcLlvmVal, dstLlvmType, false, "i2u_cast");
        } else if (typeTable->worksAsTypeF(dstTypeId)) {
            dstLlvmVal = llvmBuilder.Insert(llvm::CastInst::Create(llvm::Instruction::SIToFP, srcLlvmVal, dstLlvmType, "i2f_cast"));
        } else if (typeTable->worksAsTypeC(dstTypeId)) {
            dstLlvmVal = llvmBuilder.CreateIntCast(srcLlvmVal, dstLlvmType, false, "i2c_cast");
        } else if (typeTable->worksAsTypeB(dstTypeId)) {
            llvm::Value *zero = llvm::Constant::getNullValue(srcLlvmVal->getType());
            dstLlvmVal = llvmBuilder.CreateICmpNE(srcLlvmVal, zero, "i2b_cast");
        } else if (typeTable->worksAsTypeAnyP(dstTypeId)) {
            dstLlvmVal = llvmBuilder.CreateBitOrPointerCast(srcLlvmVal, dstLlvmType, "i2p_cast");
        }
    } else if (typeTable->worksAsTypeU(srcTypeId)) {
        if (typeTable->worksAsTypeI(dstTypeId)) {
            dstLlvmVal = llvmBuilder.CreateIntCast(srcLlvmVal, dstLlvmType, true, "u2i_cast");
        } else if (typeTable->worksAsTypeU(dstTypeId)) {
            dstLlvmVal = llvmBuilder.CreateIntCast(srcLlvmVal, dstLlvmType, false, "u2u_cast");
        } else if (typeTable->worksAsTypeF(dstTypeId)) {
            dstLlvmVal = llvmBuilder.Insert(llvm::CastInst::Create(llvm::Instruction::UIToFP, srcLlvmVal, dstLlvmType, "u2f_cast"));
        } else if (typeTable->worksAsTypeC(dstTypeId)) {
            dstLlvmVal = llvmBuilder.CreateIntCast(srcLlvmVal, dstLlvmType, false, "u2c_cast");
        } else if (typeTable->worksAsTypeB(dstTypeId)) {
            llvm::Value *zero = llvm::Constant::getNullValue(srcLlvmVal->getType());
            dstLlvmVal = llvmBuilder.CreateICmpNE(srcLlvmVal, zero, "u2b_cast");
        } else if (typeTable->worksAsTypeAnyP(dstTypeId)) {
            dstLlvmVal = llvmBuilder.CreateBitOrPointerCast(srcLlvmVal, dstLlvmType, "u2p_cast");
        }
    } else if (typeTable->worksAsTypeF(srcTypeId)) {
        if (typeTable->worksAsTypeI(dstTypeId)) {
            dstLlvmVal = llvmBuilder.Insert(llvm::CastInst::Create(llvm::Instruction::FPToSI, srcLlvmVal, dstLlvmType, "f2i_cast"));
        } else if (typeTable->worksAsTypeU(dstTypeId)) {
            dstLlvmVal = llvmBuilder.Insert(llvm::CastInst::Create(llvm::Instruction::FPToUI, srcLlvmVal, dstLlvmType, "f2u_cast"));
        } else if (typeTable->worksAsTypeF(dstTypeId)) {
            dstLlvmVal = llvmBuilder.CreateFPCast(srcLlvmVal, dstLlvmType, "f2f_cast");
        }
    } else if (typeTable->worksAsTypeC(srcTypeId)) {
        if (typeTable->worksAsTypeI(dstTypeId)) {
            dstLlvmVal = llvmBuilder.CreateIntCast(srcLlvmVal, dstLlvmType, true, "c2i_cast");
        } else if (typeTable->worksAsTypeU(dstTypeId)) {
            dstLlvmVal = llvmBuilder.CreateIntCast(srcLlvmVal, dstLlvmType, false, "c2u_cast");
        } else if (typeTable->worksAsTypeC(dstTypeId)) {
            dstLlvmVal = llvmBuilder.CreateIntCast(srcLlvmVal, dstLlvmType, false, "c2c_cast");
        } else if (typeTable->worksAsTypeB(dstTypeId)) {
            llvm::Value *zero = llvm::Constant::getNullValue(srcLlvmVal->getType());
            dstLlvmVal = llvmBuilder.CreateICmpNE(srcLlvmVal, zero, "c2b_cast");
        }
    } else if (typeTable->worksAsTypeB(srcTypeId)) {
        if (typeTable->worksAsTypeI(dstTypeId)) {
            dstLlvmVal = llvmBuilder.CreateIntCast(srcLlvmVal, dstLlvmType, false, "b2i_cast");
        } else if (typeTable->worksAsTypeU(dstTypeId)) {
            dstLlvmVal = llvmBuilder.CreateIntCast(srcLlvmVal, dstLlvmType, false, "b2u_cast");
        } else if (typeTable->worksAsTypeB(dstTypeId)) {
            dstLlvmVal = srcLlvmVal;
        }
    } else if (typeTable->worksAsTypeAnyP(srcTypeId)) {
        if (typeTable->worksAsTypeI(dstTypeId)) {
            dstLlvmVal = llvmBuilder.CreatePtrToInt(srcLlvmVal, dstLlvmType, "p2i_cast");
        } else if (typeTable->worksAsTypeU(dstTypeId)) {
            dstLlvmVal = llvmBuilder.CreatePtrToInt(srcLlvmVal, dstLlvmType, "p2u_cast");
        } else if (typeTable->worksAsTypeAnyP(dstTypeId) || typeTable->worksAsCallable(dstTypeId)) {
            dstLlvmVal = llvmBuilder.CreatePointerCast(srcLlvmVal, dstLlvmType, "p2p_cast");
        } else if (typeTable->worksAsTypeB(dstTypeId)) {
            llvm::Type *llvmTypeI = makeLlvmPrimType(TypeTable::WIDEST_I);
            if (llvmTypeI == nullptr) return nullptr;

            dstLlvmVal = llvmBuilder.CreateICmpNE(
                llvmBuilder.CreatePtrToInt(srcLlvmVal, llvmTypeI),
                llvm::ConstantInt::getNullValue(llvmTypeI),
                "p2b_cast");
        }
    } else if (typeTable->worksAsTuple(srcTypeId)) {
        if (typeTable->worksAsTuple(dstTypeId)) {
            const TypeTable::Tuple &tupSrc = *typeTable->extractTuple(srcTypeId);
            const TypeTable::Tuple &tupDst = *typeTable->extractTuple(dstTypeId);

            if (tupSrc.members.size() != tupDst.members.size()) return nullptr;

            dstLlvmVal = llvm::UndefValue::get(dstLlvmType);
            for (size_t i = 0; i < tupSrc.members.size(); ++i) {
                llvm::Value *srcLlvmMembVal = llvmBuilder.CreateExtractValue(srcLlvmVal, {(unsigned) i});

                llvm::Value *dstLlvmMembVal = makeLlvmCast(srcLlvmMembVal, tupSrc.members[i], tupDst.members[i]);
                if (dstLlvmMembVal == nullptr) return nullptr;

                dstLlvmVal = llvmBuilder.CreateInsertValue(dstLlvmVal, dstLlvmMembVal, {(unsigned) i});
            }
        }
    } else if (typeTable->worksAsCallable(srcTypeId)) {
        if (typeTable->worksAsTypeAnyP(dstTypeId) || typeTable->worksAsCallable(dstTypeId)) {
            dstLlvmVal = llvmBuilder.CreatePointerCast(srcLlvmVal, dstLlvmType, "p2p_cast");
        } else if (typeTable->worksAsTypeB(dstTypeId)) {
            llvm::Type *llvmTypeI = makeLlvmPrimType(TypeTable::WIDEST_I);
            if (llvmTypeI == nullptr) return nullptr;

            dstLlvmVal = llvmBuilder.CreateICmpNE(
                llvmBuilder.CreatePtrToInt(srcLlvmVal, llvmTypeI),
                llvm::ConstantInt::getNullValue(llvmTypeI),
                "p2b_cast");
        }
    } else {
        // these types are only castable when changing constness
        if (typeTable->isImplicitCastable(srcTypeId, dstTypeId)) {
            // no action is needed in case of a cast
            dstLlvmVal = srcLlvmVal;
        }
    }

    return dstLlvmVal;
}

bool Compiler::initLlvmTargetMachine() {
    if (targetMachine != nullptr) return true;

    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    std::string targetTriple = llvm::sys::getDefaultTargetTriple();
    llvmModule->setTargetTriple(targetTriple);

    std::string error;
    const llvm::Target *target = llvm::TargetRegistry::lookupTarget(targetTriple, error);
    if (target == nullptr) {
        llvm::errs() << error;
        return false;
    }

    const std::string cpu = "generic";
    const std::string features = "";
    const llvm::TargetOptions options;
    llvm::Optional<llvm::Reloc::Model> relocModel;
    targetMachine = target->createTargetMachine(targetTriple, cpu, features, options, relocModel);
    llvmModule->setDataLayout(targetMachine->createDataLayout());

    return true;
}