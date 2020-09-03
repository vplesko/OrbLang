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

NodeVal Codegen::performCast(const NodeVal &node, TypeTable::Id ty) {
    if (node.getType().has_value() && node.getType().value() == ty) {
        return node;
    }

    // TODO!
    // TODO don't forget str to char arr
    return NodeVal();
}

NodeVal Codegen::performCall(CodeLoc codeLoc, const FuncValue &func, const std::vector<NodeVal> &args) {
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
        LlvmVal retLlvmVal;
        retLlvmVal.type = func.retType.value();
        retLlvmVal.val = llvmBuilder.CreateCall(func.func, llvmArgValues, "call_tmp");
        return NodeVal(codeLoc, retLlvmVal);
    } else {
        llvmBuilder.CreateCall(func.func, llvmArgValues, "");
        return NodeVal(codeLoc);
    }
}

bool Codegen::performFunctionDeclaration(FuncValue &func) {
    vector<llvm::Type*> llvmArgTypes(func.argCnt());
    for (size_t i = 0; i < func.argCnt(); ++i) {
        llvmArgTypes[i] = getLlvmType(func.argTypes[i]);
        if (llvmArgTypes[i] == nullptr) return false;
    }
    
    llvm::Type *llvmRetType = func.retType.has_value() ? getLlvmType(func.retType.value()) : llvm::Type::getVoidTy(llvmContext);
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
        llvm::Type *llvmArgType = getLlvmType(func.argTypes[i]);
        if (llvmArgType == nullptr) return false;
        
        llvm::AllocaInst *llvmAlloca = makeLlvmAlloca(llvmArgType, getNameForLlvm(func.argNames[i]));
        llvmBuilder.CreateStore(&llvmFuncArg, llvmAlloca);

        LlvmVal varLlvmVal;
        varLlvmVal.type = func.argTypes[i];
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

NodeVal Codegen::performEvaluation(const NodeVal &node) {
    return evaluator->processNode(node);
}

NodeVal Codegen::promoteKnownVal(const NodeVal &node) {
    const KnownVal &known = node.getKnownVal();
    if (!node.getKnownVal().type.has_value()) {
        msgs->errorExprCannotPromote(node.getCodeLoc());
        return NodeVal();
    }
    TypeTable::Id ty = node.getKnownVal().type.value();

    llvm::Constant *llvmConst = nullptr;
    if (KnownVal::isI(known, typeTable)) {
        llvmConst = llvm::ConstantInt::get(getLlvmType(ty), KnownVal::getValueI(known, typeTable).value(), true);
    } else if (KnownVal::isU(known, typeTable)) {
        llvmConst = llvm::ConstantInt::get(getLlvmType(ty), KnownVal::getValueU(known, typeTable).value(), false);
    } else if (KnownVal::isF(known, typeTable)) {
        llvmConst = llvm::ConstantFP::get(getLlvmType(ty), KnownVal::getValueF(known, typeTable).value());
    } else if (KnownVal::isC(known, typeTable)) {
        llvmConst = llvm::ConstantInt::get(getLlvmType(ty), (uint8_t) known.c8, false);
    } else if (KnownVal::isB(known, typeTable)) {
        llvmConst = getLlvmConstB(known.b);
    } else if (KnownVal::isNull(known, typeTable)) {
        llvmConst = llvm::ConstantPointerNull::get((llvm::PointerType*)getLlvmType(ty));
    } else if (KnownVal::isStr(known, typeTable)) {
        const std::string &str = stringPool->get(known.str.value());
        llvmConst = getLlvmConstString(str);
    } else if (KnownVal::isArr(known, typeTable)) {
        // TODO!
    } else if (KnownVal::isTuple(known, typeTable)) {
        // TODO!
    }

    if (llvmConst == nullptr) {
        msgs->errorExprCannotPromote(node.getCodeLoc());
        return NodeVal();
    }

    LlvmVal llvmVal;
    llvmVal.type = ty;
    llvmVal.val = llvmConst;

    return NodeVal(node.getCodeLoc(), llvmVal);
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

llvm::Constant* Codegen::getLlvmConstString(const std::string &str) {
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

llvm::AllocaInst* Codegen::makeLlvmAlloca(llvm::Type *type, const std::string &name) {
    return llvmBuilderAlloca.CreateAlloca(type, nullptr, name);
}