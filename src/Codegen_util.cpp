#include "Codegen.h"
#include <sstream>
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
using namespace std;

Codegen::Codegen(NamePool *namePool, StringPool *stringPool, SymbolTable *symbolTable, CompileMessages *msgs)
    : namePool(namePool), stringPool(stringPool), symbolTable(symbolTable), msgs(msgs),
    llvmBuilder(llvmContext), llvmBuilderAlloca(llvmContext) {
    llvmModule = std::make_unique<llvm::Module>(llvm::StringRef("test"), llvmContext);
}

optional<NamePool::Id> Codegen::mangleName(const FuncValue &f) {
    stringstream mangle;
    mangle << namePool->get(f.name);

    mangle << "$Args";

    for (TypeTable::Id ty : f.argTypes) {
        if (getTypeTable()->isTypeDescr(ty)) {
            const TypeTable::TypeDescr &typeDescr = getTypeTable()->getTypeDescr(ty);
            
            optional<NamePool::Id> name = getTypeTable()->getTypeName(typeDescr.base);
            if (!name) {
                return nullopt;
            }

            mangle << "$" << namePool->get(name.value());

            for (TypeTable::TypeDescr::Decor d : typeDescr.decors) {
                switch (d.type) {
                    case TypeTable::TypeDescr::Decor::D_ARR:
                        mangle << "$Arr" << d.len;
                        break;
                    case TypeTable::TypeDescr::Decor::D_ARR_PTR:
                        mangle << "$ArrPtr";
                        break;
                    case TypeTable::TypeDescr::Decor::D_PTR:
                        mangle << "$Ptr";
                        break;
                    default:
                        return nullopt;
                }
            }
        } else {
            optional<NamePool::Id> name = getTypeTable()->getTypeName(ty);
            if (!name) {
                return nullopt;
            }

            mangle << "$" << namePool->get(name.value());
        }
    }

    if (f.variadic) mangle << "$Variadic";

    return namePool->add(mangle.str());
}

llvm::Type* Codegen::getType(TypeTable::Id typeId) {
    llvm::Type *llvmType = symbolTable->getTypeTable()->getType(typeId);

    if (llvmType == nullptr) {
        const TypeTable::TypeDescr &descr = symbolTable->getTypeTable()->getTypeDescr(typeId);

        llvmType = symbolTable->getTypeTable()->getType(descr.base);
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
    }

    return llvmType;
}

bool Codegen::isBool(const NodeVal &e) const {
    return (e.isUntyVal() && e.untyVal.kind == UntypedVal::Kind::kBool) ||
     (e.isLlvmVal() && getTypeTable()->worksAsTypeB(e.llvmVal.type));
}

bool Codegen::promoteUntyped(NodeVal &e, TypeTable::Id t) {
    if (!e.isUntyVal()) {
        return false;
    }

    NodeVal n(NodeVal::Kind::kLlvmVal);
    n.llvmVal.type = t;
    bool success = true;

    switch (e.untyVal.kind) {
    case UntypedVal::Kind::kBool:
        if (!getTypeTable()->worksAsTypeB(t)) {
            success = false;
        } else {
            n.llvmVal.val = getConstB(e.untyVal.val_b);
        }
        break;
    case UntypedVal::Kind::kSint:
        if ((!getTypeTable()->worksAsTypeI(t) && !getTypeTable()->worksAsTypeU(t)) || !getTypeTable()->fitsType(e.untyVal.val_si, t)) {
            success = false;
        } else {
            n.llvmVal.val = llvm::ConstantInt::get(getType(t), e.untyVal.val_si, getTypeTable()->worksAsTypeI(t));
        }
        break;
    case UntypedVal::Kind::kChar:
        if (!getTypeTable()->worksAsTypeC(t)) {
            success = false;
        } else {
            n.llvmVal.val = llvm::ConstantInt::get(getType(t), (uint8_t) e.untyVal.val_c, false);
        }
        break;
    case UntypedVal::Kind::kFloat:
        // no precision checks for float types, this makes float literals somewhat unsafe
        if (!getTypeTable()->worksAsTypeF(t)) {
            success = false;
        } else {
            n.llvmVal.val = llvm::ConstantFP::get(getType(t), e.untyVal.val_f);
        }
        break;
    case UntypedVal::Kind::kString:
        {
            const std::string &str = stringPool->get(e.untyVal.val_str);
            if (getTypeTable()->worksAsTypeStr(t)) {
                n.llvmVal.val = createString(str);
            } else if (getTypeTable()->worksAsTypeCharArrOfLen(t, UntypedVal::getStringLen(str))) {
                n.llvmVal.val = llvm::ConstantDataArray::getString(llvmContext, str, true);
            } else {
                success = false;
            }
            break;
        }
    case UntypedVal::Kind::kNull:
        if (!symbolTable->getTypeTable()->worksAsTypeAnyP(t)) {
            success = false;
        } else {
            n.llvmVal.val = llvm::ConstantPointerNull::get((llvm::PointerType*)getType(t));
        }
        break;
    default:
        success = false;
    }

    e = n;
    return success;
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

llvm::Type* Codegen::genPrimTypeF16() {
    return llvm::Type::getHalfTy(llvmContext);
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

bool Codegen::isGlobalScope() const {
    return symbolTable->inGlobalScope();
}

bool Codegen::isBlockTerminated() const {
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
        getType(getTypeTable()->getTypeCharArrOfLenId(UntypedVal::getStringLen(str))),
        true,
        llvm::GlobalValue::PrivateLinkage,
        nullptr,
        "str_lit"
    );

    llvm::Constant *arr = llvm::ConstantDataArray::getString(llvmContext, str, true);
    glob->setInitializer(arr);
    return llvm::ConstantExpr::getPointerCast(glob, getType(getTypeTable()->getTypeIdStr()));
}

bool Codegen::checkDefinedTypeOrError(TypeTable::Id type, CodeLoc codeLoc) {
    if (!getTypeTable()->isNonOpaqueType(type)) {
        if (getTypeTable()->isDataType(type)) {
            msgs->errorDataOpaqueInit(codeLoc);
            return false;
        } else {
            msgs->errorUndefinedType(codeLoc);
            return false;
        }
    }
    return true;
}

optional<Token::Type> Codegen::getStartingKeyword(const AstNode *ast) const {
    if (ast->kind != AstNode::Kind::kTuple ||
        ast->children.empty() ||
        ast->children[0]->kind != AstNode::Kind::kTerminal ||
        ast->children[0]->terminal->kind != TerminalVal::Kind::kKeyword) {
        return nullopt;
    }
    
    return ast->children[0]->terminal->keyword;
}

bool Codegen::checkStartingKeyword(const AstNode *ast, Token::Type t, bool orError) {
    optional<Token::Type> tokTy = getStartingKeyword(ast);
    if (!tokTy.has_value()) {
        if (orError) msgs->errorUnknown(ast->codeLoc);
        return false;
    } else if (tokTy.value() != t) {
        if (orError) msgs->errorUnexpectedTokenType(ast->codeLoc, tokTy.value());
        return false;
    }

    return true;
}

bool Codegen::checkTerminal(const AstNode *ast, bool orError) {
    if (ast->kind != AstNode::Kind::kTerminal) {
        if (orError) msgs->errorUnknown(ast->codeLoc);
        return false;
    }

    return true;
}

bool Codegen::checkEmptyTerminal(const AstNode *ast, bool orError) {
    if (ast->kind != AstNode::Kind::kTerminal || ast->terminal->kind != TerminalVal::Kind::kEmpty) {
        if (orError) msgs->errorUnknown(ast->codeLoc);
        return false;
    }

    return true;
}

bool Codegen::checkEllipsis(const AstNode *ast, bool orError) {
    if (ast->kind != AstNode::Kind::kTerminal ||
        ast->terminal->kind != TerminalVal::Kind::kKeyword ||
        ast->terminal->keyword != Token::T_ELLIPSIS) {
        if (orError) msgs->errorUnknown(ast->codeLoc);
        return false;
    }

    return true;
}

bool Codegen::checkNotTerminal(const AstNode *ast, bool orError) {
    if (ast->kind == AstNode::Kind::kTerminal) {
        if (orError) msgs->errorUnknown(ast->codeLoc);
        return false;
    }

    return true;
}

bool Codegen::checkBlock(const AstNode *ast, bool orError) {
    if (ast->kind == AstNode::Kind::kTerminal && ast->terminal->kind != TerminalVal::Kind::kEmpty) {
        if (orError) msgs->errorUnknown(ast->codeLoc);
        return false;
    }

    return true;
}

bool Codegen::checkExactlyChildren(const AstNode *ast, size_t n, bool orError) {
    if (ast->kind != AstNode::Kind::kTuple ||
        ast->children.size() != n) {
        if (orError) msgs->errorUnknown(ast->codeLoc);
        return false;
    }

    return true;
}

bool Codegen::checkAtLeastChildren(const AstNode *ast, size_t n, bool orError) {
    if (ast->kind != AstNode::Kind::kTuple ||
        ast->children.size() < n) {
        if (orError) msgs->errorUnknown(ast->codeLoc);
        return false;
    }

    return true;
}

bool Codegen::checkAtMostChildren(const AstNode *ast, size_t n, bool orError) {
    if (ast->kind != AstNode::Kind::kTuple ||
        ast->children.size() > n) {
        if (orError) msgs->errorUnknown(ast->codeLoc);
        return false;
    }

    return true;
}

bool Codegen::checkBetweenChildren(const AstNode *ast, std::size_t nLo, std::size_t nHi, bool orError) {
    if (ast->kind != AstNode::Kind::kTuple ||
        !between(ast->children.size(), nLo, nHi)) {
        if (orError) msgs->errorUnknown(ast->codeLoc);
        return false;
    }

    return true;
}

bool Codegen::checkValueUnbroken(CodeLoc codeLoc, const NodeVal &val, bool orError) {
    if (val.valueBroken()) {
        if (orError) msgs->errorUnknown(codeLoc);
        return false;
    }

    return true;
}

bool Codegen::checkIsType(CodeLoc codeLoc, const NodeVal &val, bool orError) {
    if (!val.isType()) {
        if (orError) msgs->errorUnknown(codeLoc);
        return false;
    }

    return true;
}

optional<NamePool::Id> Codegen::getId(const AstNode *ast, bool orError) {
    if (!checkTerminal(ast, orError)) return nullopt;

    if (ast->terminal->kind != TerminalVal::Kind::kId) {
        if (orError) msgs->errorUnknown(ast->codeLoc);
        return nullopt;
    }

    return ast->terminal->id;
}

optional<Codegen::NameTypePair> Codegen::getIdTypePair(const AstNode *ast, bool orError) {
    optional<NamePool::Id> id = getId(ast, orError);
    if (!id.has_value()) return nullopt;

    if (ast->type == nullptr) {
        if (orError) msgs->errorUnknown(ast->codeLoc);
        return nullopt;
    }
    NodeVal type = codegenType(ast->type->get());
    if (!checkIsType(ast->type->get()->codeLoc, type, true)) return nullopt;

    NameTypePair idType;
    idType.first = id.value();
    idType.second = type.type;
    return idType;
}

optional<Token::Type> Codegen::getKeyword(const AstNode *ast, bool orError) {
    if (!checkTerminal(ast, orError)) return nullopt;

    if (ast->terminal->kind != TerminalVal::Kind::kKeyword) {
        if (orError) msgs->errorUnknown(ast->codeLoc);
        return nullopt;
    }

    return ast->terminal->keyword;
}

optional<Token::Oper> Codegen::getOper(const AstNode *ast, bool orError) {
    if (!checkTerminal(ast, orError)) return nullopt;

    if (ast->terminal->kind != TerminalVal::Kind::kOper) {
        if (orError) msgs->errorUnknown(ast->codeLoc);
        return nullopt;
    }

    return ast->terminal->oper;
}

optional<UntypedVal> Codegen::getUntypedVal(const AstNode *ast, bool orError) {
    if (!checkTerminal(ast, orError)) return nullopt;

    if (ast->terminal->kind != TerminalVal::Kind::kVal) {
        if (orError) msgs->errorUnknown(ast->codeLoc);
        return nullopt;
    }

    return ast->terminal->val;
}

optional<Token::Attr> Codegen::getAttr(const AstNode *ast, bool orError) {
    if (!checkTerminal(ast, orError)) return nullopt;

    if (ast->terminal->kind != TerminalVal::Kind::kAttribute) {
        if (orError) msgs->errorUnknown(ast->codeLoc);
        return nullopt;
    }

    return ast->terminal->attribute;
}

void Codegen::printout() const {
    llvmModule->print(llvm::outs(), nullptr);
}

bool Codegen::binary(const std::string &filename) {
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