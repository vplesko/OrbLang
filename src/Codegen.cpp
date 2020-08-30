#include "Codegen.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
using namespace std;

Codegen::Codegen(Evaluator *evaluator, NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompileMessages *msgs)
    : Processor(namePool, stringPool, typeTable, symbolTable, msgs), evaluator(evaluator), llvmBuilder(llvmContext), llvmBuilderAlloca(llvmContext) {
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

NodeVal Codegen::evaluateNode(const NodeVal &node) {
    return evaluator->processNode(node);
}

llvm::Type* Codegen::getLlvmType(TypeTable::Id typeId) {
    llvm::Type *llvmType = typeTable->getType(typeId);
    if (llvmType != nullptr) return llvmType;

    if (typeTable->isTypeDescr(typeId)) {
        const TypeTable::TypeDescr &descr = typeTable->getTypeDescr(typeId);

        llvmType = getLlvmType(descr.base);
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
            llvm::Type *memberType = getLlvmType(tup.members[i]);
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

llvm::Constant* Codegen::getConstB(bool val) {
    if (val) return llvm::ConstantInt::getTrue(llvmContext);
    else return llvm::ConstantInt::getFalse(llvmContext);
}

llvm::Constant* Codegen::getConstString(const std::string &str) {
    llvm::GlobalVariable *glob = new llvm::GlobalVariable(
        *llvmModule,
        getLlvmType(typeTable->getTypeCharArrOfLenId(LiteralVal::getStringLen(str))),
        true,
        llvm::GlobalValue::PrivateLinkage,
        nullptr,
        "str_lit"
    );

    llvm::Constant *arr = llvm::ConstantDataArray::getString(llvmContext, str, true);
    glob->setInitializer(arr);
    return llvm::ConstantExpr::getPointerCast(glob, getLlvmType(typeTable->getTypeIdStr()));
}