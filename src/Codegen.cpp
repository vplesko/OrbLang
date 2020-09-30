#include "Codegen.h"
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
using namespace std;

struct ComparisonSignal {
    llvm::BasicBlock *llvmBlock = nullptr;
    llvm::PHINode *llvmPhi = nullptr;
};

Codegen::Codegen(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompileMessages *msgs, Evaluator *evaluator)
    : Processor(namePool, stringPool, typeTable, symbolTable, msgs, evaluator), llvmBuilder(llvmContext), llvmBuilderAlloca(llvmContext) {
    llvmModule = std::make_unique<llvm::Module>(llvm::StringRef("module"), llvmContext);

    llvmPmb = make_unique<llvm::PassManagerBuilder>();
    llvmPmb->OptLevel = 3;

    llvmFpm = make_unique<llvm::legacy::FunctionPassManager>(llvmModule.get());
    llvmPmb->populateFunctionPassManager(*llvmFpm);
}

void Codegen::printout() const {
    llvmModule->print(llvm::outs(), nullptr);
}

bool Codegen::binary(const std::string &filename) {
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
    llvm::TargetMachine *targetMachine = target->createTargetMachine(targetTriple, cpu, features, options, relocModel);
    llvmModule->setDataLayout(targetMachine->createDataLayout());

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

llvm::Type* Codegen::genPrimTypeBool() {
    return llvm::IntegerType::get(llvmContext, 1);
}

llvm::Type* Codegen::genPrimTypeI(unsigned bits) {
    return llvm::IntegerType::get(llvmContext, bits);
}

llvm::Type* Codegen::genPrimTypeU(unsigned bits) {
    // LLVM makes no distinction between signed and unsigned int
    return llvm::IntegerType::get(llvmContext, bits);
}

llvm::Type* Codegen::genPrimTypeC(unsigned bits) {
    return llvm::IntegerType::get(llvmContext, bits);
}

llvm::Type* Codegen::genPrimTypeF32() {
    return llvm::Type::getFloatTy(llvmContext);
}

llvm::Type* Codegen::genPrimTypeF64() {
    return llvm::Type::getDoubleTy(llvmContext);
}

llvm::Type* Codegen::genPrimTypePtr() {
    return llvm::Type::getInt8PtrTy(llvmContext);
}

NodeVal Codegen::performLoad(CodeLoc codeLoc, NamePool::Id id, NodeVal &ref) {
    if (!checkInLocalScope(codeLoc, true)) return NodeVal();

    // TODO load KnownVal if known
    if (!checkIsLlvmVal(codeLoc, ref, true)) return NodeVal();

    LlvmVal loadLlvmVal(ref.getLlvmVal().type);
    loadLlvmVal.ref = ref.getLlvmVal().ref;
    loadLlvmVal.val = llvmBuilder.CreateLoad(ref.getLlvmVal().ref, getNameForLlvm(id));
    return NodeVal(codeLoc, loadLlvmVal);
}

NodeVal Codegen::performRegister(CodeLoc codeLoc, NamePool::Id id, TypeTable::Id ty) {
    llvm::Type *llvmType = makeLlvmTypeOrError(codeLoc, ty);
    if (llvmType == nullptr) return NodeVal();
    
    LlvmVal llvmVal(ty);
    if (symbolTable->inGlobalScope()) {
        llvmVal.ref = makeLlvmGlobal(llvmType, nullptr, typeTable->worksAsTypeCn(ty), getNameForLlvm(id));
    } else {
        llvmVal.ref = makeLlvmAlloca(llvmType, getNameForLlvm(id));
    }

    return NodeVal(codeLoc, llvmVal);
}

NodeVal Codegen::performRegister(CodeLoc codeLoc, NamePool::Id id, const NodeVal &init) {
    NodeVal promo = promoteIfKnownValAndCheckIsLlvmVal(init, true);
    if (promo.isInvalid()) return NodeVal();

    TypeTable::Id ty = promo.getLlvmVal().type;
    llvm::Type *llvmType = makeLlvmTypeOrError(promo.getCodeLoc(), ty);
    if (llvmType == nullptr) return NodeVal();

    LlvmVal llvmVal(ty);
    if (symbolTable->inGlobalScope()) {
        // TODO is the cast to llvm::Constant* always correct?
        llvmVal.ref = makeLlvmGlobal(llvmType, (llvm::Constant*) promo.getLlvmVal().val, typeTable->worksAsTypeCn(ty), getNameForLlvm(id));
    } else {
        llvmVal.ref = makeLlvmAlloca(llvmType, getNameForLlvm(id));
        llvmBuilder.CreateStore(promo.getLlvmVal().val, llvmVal.ref);
    }

    return NodeVal(codeLoc, llvmVal);
}

NodeVal Codegen::performCast(CodeLoc codeLoc, const NodeVal &node, TypeTable::Id ty) {
    if (node.getType().has_value() && node.getType().value() == ty) {
        return node;
    }

    NodeVal promo = promoteIfKnownValAndCheckIsLlvmVal(node, true);
    if (promo.isInvalid()) return NodeVal();

    if (!checkInLocalScope(codeLoc, true)) return NodeVal();

    llvm::Type *llvmType = makeLlvmTypeOrError(codeLoc, ty);
    if (llvmType == nullptr) return NodeVal();

    llvm::Value *llvmValueCast = makeLlvmCast(promo.getLlvmVal().val, promo.getLlvmVal().type, llvmType, ty);
    if (llvmValueCast == nullptr) {
        msgs->errorExprCannotCast(codeLoc, promo.getLlvmVal().type, ty);
        return NodeVal();
    }

    LlvmVal llvmVal(ty);
    llvmVal.val = llvmValueCast;
    return NodeVal(codeLoc, llvmVal);
}

bool Codegen::performBlockSetUp(CodeLoc codeLoc, SymbolTable::Block &block) {
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

NodeVal Codegen::performBlockTearDown(CodeLoc codeLoc, const SymbolTable::Block &block, bool success) {
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

bool Codegen::performExit(CodeLoc codeLoc, const SymbolTable::Block &block, const NodeVal &cond) {
    if (!checkInLocalScope(codeLoc, true)) return false;

    NodeVal condPromo = promoteIfKnownValAndCheckIsLlvmVal(cond, true);
    if (condPromo.isInvalid()) return false;

    llvm::BasicBlock *llvmBlockAfter = llvm::BasicBlock::Create(llvmContext, "after");
    llvmBuilder.CreateCondBr(condPromo.getLlvmVal().val, block.blockExit, llvmBlockAfter);
    getLlvmCurrFunction()->getBasicBlockList().push_back(llvmBlockAfter);
    llvmBuilder.SetInsertPoint(llvmBlockAfter);

    return true;
}

bool Codegen::performLoop(CodeLoc codeLoc, const SymbolTable::Block &block, const NodeVal &cond) {
    if (!checkInLocalScope(codeLoc, true)) return false;

    NodeVal condPromo = promoteIfKnownValAndCheckIsLlvmVal(cond, true);
    if (condPromo.isInvalid()) return false;

    llvm::BasicBlock *llvmBlockAfter = llvm::BasicBlock::Create(llvmContext, "after");
    llvmBuilder.CreateCondBr(condPromo.getLlvmVal().val, block.blockLoop, llvmBlockAfter);
    getLlvmCurrFunction()->getBasicBlockList().push_back(llvmBlockAfter);
    llvmBuilder.SetInsertPoint(llvmBlockAfter);

    return true;
}

bool Codegen::performPass(CodeLoc codeLoc, const SymbolTable::Block &block, const NodeVal &val) {
    if (!checkInLocalScope(codeLoc, true)) return false;

    NodeVal valPromo = promoteIfKnownValAndCheckIsLlvmVal(val, true);
    if (valPromo.isInvalid()) return false;
    
    llvm::BasicBlock *llvmBlockExit = block.blockExit;
    block.phi->addIncoming(valPromo.getLlvmVal().val, llvmBuilder.GetInsertBlock());
    llvmBuilder.CreateBr(llvmBlockExit);

    return true;
}

NodeVal Codegen::performCall(CodeLoc codeLoc, const FuncValue &func, const std::vector<NodeVal> &args) {
    if (!checkInLocalScope(codeLoc, true)) return NodeVal();

    vector<llvm::Value*> llvmArgValues(args.size());
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i].isKnownVal()) {
            NodeVal llvmArg = promoteKnownVal(args[i]);
            if (llvmArg.isInvalid()) return NodeVal();
            llvmArgValues[i] = llvmArg.getLlvmVal().val;
        } else if (args[i].isLlvmVal()) {
            llvmArgValues[i] = args[i].getLlvmVal().val;
        } else {
            msgs->errorUnknown(args[i].getCodeLoc());
            return NodeVal();
        }
    }

    if (func.hasRet()) {
        LlvmVal retLlvmVal(func.retType.value());
        retLlvmVal.val = llvmBuilder.CreateCall(func.func, llvmArgValues, "call_tmp");
        return NodeVal(codeLoc, retLlvmVal);
    } else {
        llvmBuilder.CreateCall(func.func, llvmArgValues, "");
        return NodeVal(codeLoc);
    }
}

bool Codegen::performFunctionDeclaration(CodeLoc codeLoc, FuncValue &func) {
    vector<llvm::Type*> llvmArgTypes(func.argCnt());
    for (size_t i = 0; i < func.argCnt(); ++i) {
        llvmArgTypes[i] = makeLlvmTypeOrError(codeLoc, func.argTypes[i]);
        if (llvmArgTypes[i] == nullptr) return false;
    }
    
    llvm::Type *llvmRetType = func.retType.has_value() ? makeLlvmTypeOrError(codeLoc, func.retType.value()) : llvm::Type::getVoidTy(llvmContext);
    if (llvmRetType == nullptr) return false;

    llvm::FunctionType *llvmFuncType = llvm::FunctionType::get(llvmRetType, llvmArgTypes, func.variadic);

    func.func = llvm::Function::Create(llvmFuncType, llvm::Function::ExternalLinkage, getNameForLlvm(func.name), llvmModule.get());

    return true;
}

bool Codegen::performFunctionDefinition(const NodeVal &args, const NodeVal &body, FuncValue &func) {
    BlockControl blockCtrl(*symbolTable, func);

    llvmBuilderAlloca.SetInsertPoint(llvm::BasicBlock::Create(llvmContext, "alloca", func.func));

    llvm::BasicBlock *llvmBlockBody = llvm::BasicBlock::Create(llvmContext, "entry", func.func);
    llvmBuilder.SetInsertPoint(llvmBlockBody);

    size_t i = 0;
    for (auto &llvmFuncArg : func.func->args()) {
        llvm::Type *llvmArgType = makeLlvmTypeOrError(args.getChild(i).getCodeLoc(), func.argTypes[i]);
        if (llvmArgType == nullptr) return false;
        
        llvm::AllocaInst *llvmAlloca = makeLlvmAlloca(llvmArgType, getNameForLlvm(func.argNames[i]));
        llvmBuilder.CreateStore(&llvmFuncArg, llvmAlloca);

        LlvmVal varLlvmVal(func.argTypes[i]);
        varLlvmVal.ref = llvmAlloca;
        NodeVal varNodeVal(args.getChild(i).getCodeLoc(), varLlvmVal);
        symbolTable->addVar(func.argNames[i], move(varNodeVal));

        ++i;
    }

    if (!processChildNodes(body)) {
        func.func->eraseFromParent();
        return false;
    }

    llvmBuilderAlloca.CreateBr(llvmBlockBody);

    if (!func.hasRet() && !isLlvmBlockTerminated())
        llvmBuilder.CreateRetVoid();

    if (llvm::verifyFunction(*func.func, &llvm::errs())) cerr << endl;
    llvmFpm->run(*func.func);

    return true;
}

bool Codegen::performRet(CodeLoc codeLoc) {
    llvmBuilder.CreateRetVoid();
    return true;
}

bool Codegen::performRet(CodeLoc codeLoc, const NodeVal &node) {
    NodeVal promo = promoteIfKnownValAndCheckIsLlvmVal(node, true);
    if (promo.isInvalid()) return false;

    llvmBuilder.CreateRet(promo.getLlvmVal().val);
    return true;
}

NodeVal Codegen::performOperUnary(CodeLoc codeLoc, const NodeVal &oper, Oper op) {
    if (!checkInLocalScope(codeLoc, true)) return NodeVal();
    
    NodeVal promo = promoteIfKnownValAndCheckIsLlvmVal(oper, true);
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

NodeVal Codegen::performOperUnaryDeref(CodeLoc codeLoc, const NodeVal &oper) {
    if (!checkInLocalScope(codeLoc, true)) return NodeVal();
    
    NodeVal promo = promoteIfKnownValAndCheckIsLlvmVal(oper, true);
    if (promo.isInvalid()) return NodeVal();

    TypeTable::Id operTy = promo.getLlvmVal().type;
    llvm::Value *llvmIn = promo.getLlvmVal().val;

    optional<TypeTable::Id> typeId = typeTable->addTypeDerefOf(operTy);
    if (!typeId.has_value()) {
        msgs->errorUnknown(codeLoc);
        return NodeVal();
    }

    LlvmVal llvmVal(typeId.value());
    llvmVal.val = llvmBuilder.CreateLoad(llvmIn, "deref_tmp");
    llvmVal.ref = llvmIn;
    return NodeVal(codeLoc, llvmVal);
}

void* Codegen::performOperComparisonSetUp(CodeLoc codeLoc, size_t opersCnt) {
    if (!checkInLocalScope(codeLoc, true)) return nullptr;

    llvm::BasicBlock *llvmBlockCurr = llvmBuilder.GetInsertBlock();

    llvm::BasicBlock *llvmBlockRes = llvm::BasicBlock::Create(llvmContext, "comparison");
    llvmBuilder.SetInsertPoint(llvmBlockRes);
    llvm::PHINode *llvmPhiRes = llvmBuilder.CreatePHI(makeLlvmPrimType(TypeTable::P_BOOL), (unsigned) opersCnt);

    llvmBuilder.SetInsertPoint(llvmBlockCurr);

    ComparisonSignal *compSignal = new ComparisonSignal;
    compSignal->llvmBlock = llvmBlockRes;
    compSignal->llvmPhi = llvmPhiRes;
    return compSignal;
}

optional<bool> Codegen::performOperComparison(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op, void *signal) {
    ComparisonSignal *compSignal = (ComparisonSignal*) signal;
    
    NodeVal lhsPromo = promoteIfKnownValAndCheckIsLlvmVal(lhs, true);
    if (lhsPromo.isInvalid()) return nullopt;
    
    NodeVal rhsPromo = promoteIfKnownValAndCheckIsLlvmVal(rhs, true);
    if (rhsPromo.isInvalid()) return nullopt;
    
    bool isTypeI = typeTable->worksAsTypeI(lhsPromo.getType().value());
    bool isTypeU = typeTable->worksAsTypeU(lhsPromo.getType().value());
    bool isTypeC = typeTable->worksAsTypeC(lhsPromo.getType().value());
    bool isTypeF = typeTable->worksAsTypeF(lhsPromo.getType().value());
    bool isTypeP = typeTable->worksAsTypeAnyP(lhsPromo.getType().value());
    bool isTypeB = typeTable->worksAsTypeB(lhsPromo.getType().value());

    llvm::Value *llvmValueRes = nullptr;

    switch (op) {
    case Oper::EQ:
        if (isTypeI || isTypeU || isTypeC || isTypeB) {
            llvmValueRes = llvmBuilder.CreateICmpEQ(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "cmp_eq_tmp");
        } else if (isTypeF) {
            llvmValueRes = llvmBuilder.CreateFCmpOEQ(lhsPromo.getLlvmVal().val, rhsPromo.getLlvmVal().val, "fcmp_eq_tmp");
        } else if (isTypeP) {
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
        } else if (isTypeP) {
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
    }

    if (llvmValueRes == nullptr) {
        msgs->errorUnknown(rhs.getCodeLoc());
        return nullopt;
    }

    compSignal->llvmPhi->addIncoming(getLlvmConstB(false), llvmBuilder.GetInsertBlock());

    llvm::BasicBlock *llvmBlockNext = llvm::BasicBlock::Create(llvmContext, "next", getLlvmCurrFunction());
    llvmBuilder.CreateCondBr(llvmValueRes, llvmBlockNext, compSignal->llvmBlock);

    llvmBuilder.SetInsertPoint(llvmBlockNext);

    return false;
}

NodeVal Codegen::performOperComparisonTearDown(CodeLoc codeLoc, bool success, void *signal) {
    ComparisonSignal *compSignal = (ComparisonSignal*) signal;
    if (!success) {
        if (compSignal->llvmPhi != nullptr) compSignal->llvmPhi->eraseFromParent();
        return NodeVal();
    }

    compSignal->llvmPhi->addIncoming(getLlvmConstB(true), llvmBuilder.GetInsertBlock());
    llvmBuilder.CreateBr(compSignal->llvmBlock);

    getLlvmCurrFunction()->getBasicBlockList().push_back(compSignal->llvmBlock);
    llvmBuilder.SetInsertPoint(compSignal->llvmBlock);

    LlvmVal llvmVal(typeTable->getPrimTypeId(TypeTable::P_BOOL));
    llvmVal.val = compSignal->llvmPhi;
    return NodeVal(codeLoc, llvmVal);
}

NodeVal Codegen::performOperAssignment(CodeLoc codeLoc, NodeVal &lhs, const NodeVal &rhs) {
    if (!checkInLocalScope(codeLoc, true)) return NodeVal();

    if (!checkIsLlvmVal(lhs, true)) return NodeVal();
    
    NodeVal rhsPromo = promoteIfKnownValAndCheckIsLlvmVal(rhs, true);
    if (rhsPromo.isInvalid()) return NodeVal();

    llvmBuilder.CreateStore(rhsPromo.getLlvmVal().val, lhs.getLlvmVal().ref);

    LlvmVal llvmVal(lhs.getType().value());
    llvmVal.val = rhsPromo.getLlvmVal().val;
    llvmVal.ref = lhs.getLlvmVal().ref;
    return NodeVal(lhs.getCodeLoc(), llvmVal);
}

NodeVal Codegen::performOperIndex(CodeLoc codeLoc, NodeVal &base, const NodeVal &ind, TypeTable::Id resTy) {
    if (!checkInLocalScope(codeLoc, true)) return NodeVal();
    
    NodeVal basePromo = promoteIfKnownValAndCheckIsLlvmVal(base, true);
    if (basePromo.isInvalid()) return NodeVal();
    
    NodeVal indPromo = promoteIfKnownValAndCheckIsLlvmVal(ind, true);
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
    } else {
        msgs->errorInternal(codeLoc);
        return NodeVal();
    }
    return NodeVal(codeLoc, llvmVal);
}

NodeVal Codegen::performOperMember(CodeLoc codeLoc, NodeVal &base, std::uint64_t ind, TypeTable::Id resTy) {
    if (!checkInLocalScope(codeLoc, true)) return NodeVal();
    
    NodeVal basePromo = promoteIfKnownValAndCheckIsLlvmVal(base, true);
    if (basePromo.isInvalid()) return NodeVal();

    LlvmVal llvmVal(resTy);
    if (basePromo.hasRef()) {
        llvmVal.ref = llvmBuilder.CreateStructGEP(basePromo.getLlvmVal().ref, (unsigned) ind);
        llvmVal.val = llvmBuilder.CreateLoad(llvmVal.ref, "dot_tmp");
    } else {
        llvmVal.val = llvmBuilder.CreateExtractValue(basePromo.getLlvmVal().val, {(unsigned) ind}, "dot_tmp");
    }
    return NodeVal(codeLoc, llvmVal);
}

NodeVal Codegen::performOperRegular(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op) {
    if (!checkInLocalScope(codeLoc, true)) return NodeVal();
    
    NodeVal lhsPromo = promoteIfKnownValAndCheckIsLlvmVal(lhs, true);
    if (lhs.isInvalid()) return NodeVal();
    
    NodeVal rhsPromo = promoteIfKnownValAndCheckIsLlvmVal(rhs, true);
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
    }
    
    if (llvmVal.val == nullptr) {
        msgs->errorUnknown(rhs.getCodeLoc());
        return NodeVal();
    }

    return NodeVal(rhs.getCodeLoc(), llvmVal);
}

NodeVal Codegen::performTuple(CodeLoc codeLoc, TypeTable::Id ty, const std::vector<NodeVal> &membs) {
    if (!checkInLocalScope(codeLoc, true)) return NodeVal();

    llvm::StructType *llvmTupType = (llvm::StructType*) makeLlvmTypeOrError(codeLoc, ty);
    if (llvmTupType == nullptr) return NodeVal();

    vector<llvm::Value*> llvmMembVals;
    llvmMembVals.reserve(membs.size());
    for (const NodeVal &memb : membs) {
        NodeVal promo = promoteIfKnownValAndCheckIsLlvmVal(memb, true);
        if (promo.isInvalid()) return NodeVal();
        llvmMembVals.push_back(promo.getLlvmVal().val);
    }

    llvm::Value *llvmTupVal = llvm::UndefValue::get(llvmTupType);
    for (size_t i = 0; i < llvmMembVals.size(); ++i) {
        llvmTupVal = llvmBuilder.CreateInsertValue(llvmTupVal, llvmMembVals[i], {(unsigned) i});
    }

    LlvmVal llvmVal(ty);
    llvmVal.val = llvmTupVal;
    return NodeVal(codeLoc, llvmVal);
}

NodeVal Codegen::promoteKnownVal(CodeLoc codeLoc, const KnownVal &known) {
    if (!known.type.has_value()) {
        msgs->errorExprCannotPromote(codeLoc);
        return NodeVal();
    }
    TypeTable::Id ty = known.type.value();

    llvm::Constant *llvmConst = nullptr;
    if (KnownVal::isI(known, typeTable)) {
        llvmConst = llvm::ConstantInt::get(makeLlvmType(ty), KnownVal::getValueI(known, typeTable).value(), true);
    } else if (KnownVal::isU(known, typeTable)) {
        llvmConst = llvm::ConstantInt::get(makeLlvmType(ty), KnownVal::getValueU(known, typeTable).value(), false);
    } else if (KnownVal::isF(known, typeTable)) {
        llvmConst = llvm::ConstantFP::get(makeLlvmType(ty), KnownVal::getValueF(known, typeTable).value());
    } else if (KnownVal::isC(known, typeTable)) {
        llvmConst = llvm::ConstantInt::get(makeLlvmType(ty), (uint8_t) known.c8, false);
    } else if (KnownVal::isB(known, typeTable)) {
        llvmConst = getLlvmConstB(known.b);
    } else if (KnownVal::isNull(known, typeTable)) {
        llvmConst = llvm::ConstantPointerNull::get((llvm::PointerType*)makeLlvmType(ty));
    } else if (KnownVal::isStr(known, typeTable)) {
        const std::string &str = stringPool->get(known.str.value());
        llvmConst = makeLlvmConstString(str);
    } else if (KnownVal::isArr(known, typeTable)) {
        llvm::ArrayType *llvmArrayType = (llvm::ArrayType*) makeLlvmTypeOrError(codeLoc, known.type.value());
        if (llvmArrayType == nullptr) return NodeVal();

        vector<llvm::Constant*> llvmConsts;
        llvmConsts.reserve(known.elems.size());
        for (const KnownVal &elem : known.elems) {
            NodeVal elemPromo = promoteKnownVal(codeLoc, elem);
            if (elemPromo.isInvalid()) return NodeVal();
            llvmConsts.push_back((llvm::Constant*) elemPromo.getLlvmVal().val);
        }

        llvmConst = llvm::ConstantArray::get(llvmArrayType, llvmConsts);
    } else if (KnownVal::isTuple(known, typeTable)) {
        vector<llvm::Constant*> llvmConsts;
        llvmConsts.reserve(known.elems.size());
        for (const KnownVal &elem : known.elems) {
            NodeVal elemPromo = promoteKnownVal(codeLoc, elem);
            if (elemPromo.isInvalid()) return NodeVal();
            llvmConsts.push_back((llvm::Constant*) elemPromo.getLlvmVal().val);
        }

        llvmConst = llvm::ConstantStruct::getAnon(llvmContext, llvmConsts);
    }

    if (llvmConst == nullptr) {
        msgs->errorExprCannotPromote(codeLoc);
        return NodeVal();
    }

    LlvmVal llvmVal(ty);
    llvmVal.val = llvmConst;

    return NodeVal(codeLoc, llvmVal);
}

NodeVal Codegen::promoteKnownVal(const NodeVal &node) {
    return promoteKnownVal(node.getCodeLoc(), node.getKnownVal());
}

NodeVal Codegen::promoteIfKnownValAndCheckIsLlvmVal(const NodeVal &node, bool orError) {
    NodeVal promo = node.isKnownVal() ? promoteKnownVal(node) : node;
    if (promo.isInvalid()) return NodeVal();
    if (!checkIsLlvmVal(promo, orError)) return NodeVal();
    return promo;
}

bool Codegen::checkIsLlvmVal(CodeLoc codeLoc, const NodeVal &node, bool orError) {
    if (!node.isLlvmVal()) {
        if (orError) msgs->errorUnknown(codeLoc);
        return false;
    }
    return true;
}

string Codegen::getNameForLlvm(NamePool::Id name) const {
    // LLVM is smart enough to put quotes around IDs with special chars, but let's keep this method in anyway.
    return namePool->get(name);
}

bool Codegen::isLlvmBlockTerminated() const {
    return !llvmBuilder.GetInsertBlock()->empty() && llvmBuilder.GetInsertBlock()->back().isTerminator();
}

llvm::Constant* Codegen::getLlvmConstB(bool val) {
    if (val) return llvm::ConstantInt::getTrue(llvmContext);
    else return llvm::ConstantInt::getFalse(llvmContext);
}

llvm::Constant* Codegen::makeLlvmConstString(const std::string &str) {
    return llvmBuilder.CreateGlobalStringPtr(str, "str_lit");
}

llvm::Type* Codegen::makeLlvmType(TypeTable::Id typeId) {
    llvm::Type *llvmType = typeTable->getType(typeId);
    if (llvmType != nullptr) return llvmType;

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

        llvm::StructType *structType = llvm::StructType::create(llvmContext, "tuple");

        vector<llvm::Type*> memberTypes(tup.members.size());
        for (size_t i = 0; i < tup.members.size(); ++i) {
            llvm::Type *memberType = makeLlvmType(tup.members[i]);
            if (memberType == nullptr) return nullptr;
            memberTypes[i] = memberType;
        }

        structType->setBody(memberTypes);

        llvmType = structType;
    }

    // supported primitive type are codegened at the start of compilation
    // in case of id or type, nullptr is returned    
    typeTable->setType(typeId, llvmType);
    return llvmType;
}

llvm::Type* Codegen::makeLlvmTypeOrError(CodeLoc codeLoc, TypeTable::Id typeId) {
    llvm::Type *ret = makeLlvmType(typeId);
    if (ret == nullptr) {
        msgs->errorUnknown(codeLoc);
    }
    return ret;
}

llvm::GlobalValue* Codegen::makeLlvmGlobal(llvm::Type *type, llvm::Constant *init, bool isConstant, const std::string &name) {
    if (init == nullptr) init = llvm::Constant::getNullValue(type);

    return new llvm::GlobalVariable(
        *llvmModule,
        type,
        isConstant,
        llvm::GlobalValue::PrivateLinkage,
        init,
        name);
}

llvm::AllocaInst* Codegen::makeLlvmAlloca(llvm::Type *type, const std::string &name) {
    return llvmBuilderAlloca.CreateAlloca(type, nullptr, name);
}

llvm::Value* Codegen::makeLlvmCast(llvm::Value *srcLlvmVal, TypeTable::Id srcTypeId, llvm::Type *dstLlvmType, TypeTable::Id dstTypeId) {
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
        } else if (typeTable->worksAsTypeAnyP(dstTypeId)) {
            dstLlvmVal = llvmBuilder.CreatePointerCast(srcLlvmVal, dstLlvmType, "p2p_cast");
        } else if (typeTable->worksAsTypeB(dstTypeId)) {
            llvm::Type *llvmTypeI = makeLlvmPrimType(TypeTable::WIDEST_I);
            if (llvmTypeI == nullptr) return nullptr;

            dstLlvmVal = llvmBuilder.CreateICmpNE(
                llvmBuilder.CreatePtrToInt(srcLlvmVal, llvmTypeI),
                llvm::ConstantInt::getNullValue(llvmTypeI),
                "p2b_cast");
        }
    } else if (typeTable->worksAsTypeArr(srcTypeId) || typeTable->worksAsTuple(srcTypeId)) {
        // tuples and arrs are only castable when changing constness
        if (typeTable->isImplicitCastable(srcTypeId, dstTypeId)) {
            // no action is needed in case of a cast
            dstLlvmVal = srcLlvmVal;
        }
    }

    return dstLlvmVal;
}