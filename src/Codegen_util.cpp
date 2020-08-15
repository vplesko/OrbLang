#include "Codegen.h"
#include <sstream>
#include "Evaluator.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
using namespace std;

Codegen::Codegen(NamePool *namePool, StringPool *stringPool, SymbolTable *symbolTable, CompileMessages *msgs)
    : CodeProcessor(namePool, stringPool, symbolTable, msgs),
    llvmBuilder(llvmContext), llvmBuilderAlloca(llvmContext) {
    llvmModule = std::make_unique<llvm::Module>(llvm::StringRef("test"), llvmContext);

    llvmPMB = make_unique<llvm::PassManagerBuilder>();
    llvmPMB->OptLevel = 3;

    llvmFPM = make_unique<llvm::legacy::FunctionPassManager>(llvmModule.get());
    llvmPMB->populateFunctionPassManager(*llvmFPM);
}

bool Codegen::mangleName(stringstream &ss, TypeTable::Id ty) {
    if (getTypeTable()->isTypeDescr(ty)) {
        const TypeTable::TypeDescr &typeDescr = getTypeTable()->getTypeDescr(ty);
    
        if (!mangleName(ss, typeDescr.base)) return false;

        for (TypeTable::TypeDescr::Decor d : typeDescr.decors) {
            switch (d.type) {
            case TypeTable::TypeDescr::Decor::D_ARR:
                ss << "$Arr" << d.len;
                break;
            case TypeTable::TypeDescr::Decor::D_ARR_PTR:
                ss << "$ArrPtr";
                break;
            case TypeTable::TypeDescr::Decor::D_PTR:
                ss << "$Ptr";
                break;
            default:
                return false;
            }
        }
    } else if (getTypeTable()->isTuple(ty)) {
        const TypeTable::Tuple &tup = getTypeTable()->getTuple(ty);

        ss << "$Tup";
        ss << "$" << tup.members.size();

        for (auto it : tup.members) {
            ss << "$";
            if (!mangleName(ss, it)) return false;
        }
    } else {
        optional<NamePool::Id> name = getTypeTable()->getTypeName(ty);
        if (!name) {
            return false;
        }

        ss << "$" << namePool->get(name.value());
    }

    return true;
}

optional<NamePool::Id> Codegen::mangleName(const FuncValue &f) {
    stringstream mangle;
    mangle << namePool->get(f.name);

    mangle << "$Args";

    for (TypeTable::Id ty : f.argTypes) {
        if (!mangleName(mangle, ty)) return nullopt;
    }

    if (f.variadic) mangle << "$Variadic";

    return namePool->add(mangle.str());
}

llvm::Type* Codegen::getLlvmType(TypeTable::Id typeId) {
    llvm::Type *llvmType = getTypeTable()->getType(typeId);
    if (llvmType != nullptr) return llvmType;

    // primitive type are codegened at the start of compilation
    if (getTypeTable()->isTypeDescr(typeId)) {
        const TypeTable::TypeDescr &descr = symbolTable->getTypeTable()->getTypeDescr(typeId);

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

            if (llvmType == nullptr) return nullptr;
        }

        symbolTable->getTypeTable()->setType(typeId, llvmType);
    } else if (getTypeTable()->isTuple(typeId)) {
        const TypeTable::Tuple &tup = getTypeTable()->getTuple(typeId);

        llvm::StructType *structType = llvm::StructType::create(llvmContext, "tuple");
        getTypeTable()->setType(typeId, structType);

        vector<llvm::Type*> memberTypes(tup.members.size());
        for (size_t i = 0; i < tup.members.size(); ++i) {
            llvm::Type *memberType = getLlvmType(tup.members[i]);
            memberTypes[i] = memberType;
        }

        structType->setBody(memberTypes);

        llvmType = structType;
    }

    return llvmType;
}

bool Codegen::promoteKnownVal(NodeVal &e, TypeTable::Id t) {
    if (!e.isKnownVal() || !KnownVal::isImplicitCastable(e.knownVal, t, stringPool, getTypeTable())) {
        return false;
    }

    NodeVal n(NodeVal::Kind::kLlvmVal);
    n.llvmVal.type = t;
    bool success = true;

    if (KnownVal::isI(e.knownVal, getTypeTable())) {
        n.llvmVal.val = llvm::ConstantInt::get(getLlvmType(t), KnownVal::getValueI(e.knownVal, getTypeTable()).value(), true);
    } else if (KnownVal::isU(e.knownVal, getTypeTable())) {
        n.llvmVal.val = llvm::ConstantInt::get(getLlvmType(t), KnownVal::getValueU(e.knownVal, getTypeTable()).value(), false);
    } else if (KnownVal::isF(e.knownVal, getTypeTable())) {
        n.llvmVal.val = llvm::ConstantFP::get(getLlvmType(t), KnownVal::getValueF(e.knownVal, getTypeTable()).value());
    } else if (KnownVal::isC(e.knownVal, getTypeTable())) {
        n.llvmVal.val = llvm::ConstantInt::get(getLlvmType(t), (uint8_t) e.knownVal.c8, false);
    } else if (KnownVal::isB(e.knownVal, getTypeTable())) {
        n.llvmVal.val = getConstB(e.knownVal.b);
    } else if (KnownVal::isStr(e.knownVal, getTypeTable())) {
        const std::string &str = stringPool->get(e.knownVal.str);
        if (getTypeTable()->worksAsTypeStr(t)) {
            n.llvmVal.val = createString(str);
        } else {
            n.llvmVal.val = llvm::ConstantDataArray::getString(llvmContext, str, true);
        }
    } else if (KnownVal::isNull(e.knownVal, getTypeTable())) {
        n.llvmVal.val = llvm::ConstantPointerNull::get((llvm::PointerType*)getLlvmType(t));
    } else {
        success = false;
    }

    e = n;
    return success;
}

bool Codegen::promoteKnownVal(NodeVal &e) {
    if (!e.isKnownVal()) return false;

    return promoteKnownVal(e, e.knownVal.type);
}

llvm::Value* Codegen::getConstB(bool val) {
    if (val) return llvm::ConstantInt::getTrue(llvmContext);
    else return llvm::ConstantInt::getFalse(llvmContext);
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

llvm::AllocaInst* Codegen::createAlloca(llvm::Type *type, const string &name) {
    return llvmBuilderAlloca.CreateAlloca(type, 0, name);
}

bool Codegen::isLlvmBlockTerminated() const {
    return !llvmBuilder.GetInsertBlock()->empty() && llvmBuilder.GetInsertBlock()->back().isTerminator();
}

llvm::GlobalValue* Codegen::createGlobal(llvm::Type *type, llvm::Constant *init, bool isConstant, const std::string &name) {
    if (init == nullptr) {
        // llvm demands global vars be initialized, but by default we don't init them
        init = llvm::Constant::getNullValue(type);
    }

    return new llvm::GlobalVariable(
        *llvmModule,
        type,
        isConstant,
        llvm::GlobalValue::PrivateLinkage,
        init,
        name);
}

llvm::Constant* Codegen::createString(const std::string &str) {
    llvm::GlobalVariable *glob = new llvm::GlobalVariable(
        *llvmModule,
        getLlvmType(getTypeTable()->getTypeCharArrOfLenId(LiteralVal::getStringLen(str))),
        true,
        llvm::GlobalValue::PrivateLinkage,
        nullptr,
        "str_lit"
    );

    llvm::Constant *arr = llvm::ConstantDataArray::getString(llvmContext, str, true);
    glob->setInitializer(arr);
    return llvm::ConstantExpr::getPointerCast(glob, getLlvmType(getTypeTable()->getTypeIdStr()));
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
    llvm::Optional<llvm::Reloc::Model> RM = llvm::Optional<llvm::Reloc::Model>();
    llvm::TargetMachine *targetMachine = target->createTargetMachine(targetTriple, cpu, features, options, RM);

    llvmModule->setDataLayout(targetMachine->createDataLayout());

    std::error_code EC;
    llvm::raw_fd_ostream dest(filename, EC, llvm::sys::fs::F_None);
    if (EC) {
        llvm::errs() << "Could not open file: " << EC.message();
        return false;
    }

    llvm::legacy::PassManager MPM;
    llvmPMB->populateModulePassManager(MPM);

    llvm::CodeGenFileType fileType = llvm::CGFT_ObjectFile;

    bool fail = targetMachine->addPassesToEmitFile(MPM, dest, nullptr, fileType);
    if (fail) {
        llvm::errs() << "Target machine can't emit to this file type!";
        return false;
    }

    MPM.run(*llvmModule);
    dest.flush();

    return true;
}