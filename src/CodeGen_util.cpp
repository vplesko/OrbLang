#include "Codegen.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
using namespace std;

CodeGen::CodeGen(NamePool *namePool, SymbolTable *symbolTable) : namePool(namePool), symbolTable(symbolTable), 
        llvmBuilder(llvmContext), llvmBuilderAlloca(llvmContext), panic(false) {
    llvmModule = std::make_unique<llvm::Module>(llvm::StringRef("test"), llvmContext);
}

llvm::Type* CodeGen::getType(TypeTable::Id typeId) {
    llvm::Type *llvmType = symbolTable->getTypeTable()->getType(typeId);

    if (llvmType == nullptr) {
        const TypeTable::TypeDescr &descr = symbolTable->getTypeTable()->getTypeDescr(typeId);

        llvmType = symbolTable->getTypeTable()->getType(descr.base);
        if (broken(llvmType)) return nullptr;

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
                panic = true;
                return nullptr;
            }

            if (broken(llvmType)) return nullptr;
        }

        symbolTable->getTypeTable()->setType(typeId, llvmType);
    }

    return llvmType;
}

bool CodeGen::valueBroken(const ExprGenPayload &e) {
    if (e.val == nullptr && !e.isLitVal()) panic = true;
    return panic;
}

bool CodeGen::valBroken(const ExprGenPayload &e) {
    if (e.val == nullptr) panic = true;
    return panic;
}

bool CodeGen::refBroken(const ExprGenPayload &e) {
    if (e.ref == nullptr) panic = true;
    return panic;
}

bool CodeGen::promoteLiteral(ExprGenPayload &e, TypeTable::Id t) {
    if (!e.isLitVal()) {
        panic = true;
        return false;
    }

    switch (e.litVal.type) {
    case LiteralVal::T_BOOL:
        if (!TypeTable::isTypeB(t)) {
            panic = true;
        } else {
            e.val = getConstB(e.litVal.val_b);
        }
        break;
    case LiteralVal::T_SINT:
        if ((!TypeTable::isTypeI(t) && !TypeTable::isTypeU(t)) || !TypeTable::fitsType(e.litVal.val_si, t)) {
            panic = true;
        } else {
            e.val = llvm::ConstantInt::get(getType(t), e.litVal.val_si, TypeTable::isTypeI(t));
        }
        break;
    case LiteralVal::T_FLOAT:
        // no precision checks for float types, this makes float literals somewhat unsafe
        if (!TypeTable::isTypeF(t)) {
            panic = true;
        } else {
            e.val = llvm::ConstantFP::get(getType(t), e.litVal.val_f);
        }
        break;
    case LiteralVal::T_NULL:
        if (!symbolTable->getTypeTable()->isTypeAnyP(t)) {
            panic = true;
        } else {
            e.val = llvm::ConstantPointerNull::get((llvm::PointerType*)getType(t));
        }
        break;
    default:
        panic = true;
    }

    e.resetLitVal();
    e.type = t;
    return !panic;
}

llvm::Value* CodeGen::getConstB(bool val) {
    if (val) return llvm::ConstantInt::getTrue(llvmContext);
    else return llvm::ConstantInt::getFalse(llvmContext);
}

llvm::Type* CodeGen::genPrimTypeBool() {
    return llvm::IntegerType::get(llvmContext, 1);
}

llvm::Type* CodeGen::genPrimTypeI(unsigned bits) {
    return llvm::IntegerType::get(llvmContext, bits);
}

llvm::Type* CodeGen::genPrimTypeU(unsigned bits) {
    // LLVM makes no distinction between signed and unsigned int
    return llvm::IntegerType::get(llvmContext, bits);
}

llvm::Type* CodeGen::genPrimTypeF16() {
    return llvm::Type::getHalfTy(llvmContext);
}

llvm::Type* CodeGen::genPrimTypeF32() {
    return llvm::Type::getFloatTy(llvmContext);
}

llvm::Type* CodeGen::genPrimTypeF64() {
    return llvm::Type::getDoubleTy(llvmContext);
}

llvm::Type* CodeGen::genPrimTypePtr() {
    return llvm::Type::getInt8PtrTy(llvmContext);
}

llvm::AllocaInst* CodeGen::createAlloca(llvm::Type *type, const string &name) {
    return llvmBuilderAlloca.CreateAlloca(type, 0, name);
}

bool CodeGen::isGlobalScope() const {
    return symbolTable->inGlobalScope();
}

bool CodeGen::isBlockTerminated() const {
    return !llvmBuilder.GetInsertBlock()->empty() && llvmBuilder.GetInsertBlock()->back().isTerminator();
}

llvm::GlobalValue* CodeGen::createGlobal(llvm::Type *type, llvm::Constant *init, const std::string &name) {
    if (init == nullptr) {
        // llvm demands global vars be initialized, but by deafult we don't init them
        // TODO this is very hacky
        init = llvm::ConstantInt::get(type, 0);
    }

    return new llvm::GlobalVariable(
        *llvmModule,
        type,
        false,
        // TODO revise when implementing multi-file compilation
        llvm::GlobalValue::PrivateLinkage,
        init,
        name);
}

void CodeGen::printout() const {
    llvmModule->print(llvm::outs(), nullptr);
}

bool CodeGen::binary(const std::string &filename) {
    // TODO add optimizer passes
    
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
    llvm::Optional<llvm::Reloc::Model> RM = llvm::Optional<llvm::Reloc::Model>();
    llvm::TargetMachine *targetMachine = target->createTargetMachine(targetTriple, cpu, features, options, RM);

    llvmModule->setDataLayout(targetMachine->createDataLayout());

    std::error_code EC;
    llvm::raw_fd_ostream dest(filename, EC, llvm::sys::fs::F_None);
    if (EC) {
        llvm::errs() << "Could not open file: " << EC.message();
        return false;
    }

    llvm::legacy::PassManager pass;
    llvm::TargetMachine::CodeGenFileType fileType = llvm::TargetMachine::CGFT_ObjectFile;

    bool fail = targetMachine->addPassesToEmitFile(pass, dest, nullptr, fileType);
    if (fail) {
        llvm::errs() << "Target machine can't emit to this file type!";
        return false;
    }

    pass.run(*llvmModule);
    dest.flush();

    return true;
}