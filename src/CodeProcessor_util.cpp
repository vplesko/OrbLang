#include "CodeProcessor.h"
using namespace std;

CodeProcessor::CodeProcessor(NamePool *namePool, StringPool *stringPool, SymbolTable *symbolTable, CompileMessages *msgs)
    : namePool(namePool), stringPool(stringPool), symbolTable(symbolTable), msgs(msgs) {
}

SymbolTable::Block* CodeProcessor::getBlock(CodeLoc codeLoc, NamePool::Id name, bool orError) {
    SymbolTable::Block *block = symbolTable->getBlock(name);
    if (block == nullptr && orError)
        msgs->errorBlockNotFound(codeLoc, name);
    return block;
}

bool CodeProcessor::verifyTypeAnnotationMatch(const NodeVal &val, const AstNode *typeAst, bool orError) {
    optional<TypeTable::Id> typeOpt = getType(typeAst, true);
    if (!typeOpt.has_value()) return false;

    switch (val.kind) {
    case NodeVal::Kind::kLlvmVal:
        if (val.llvmVal.type != typeOpt.value()) {
            if (orError) msgs->errorMismatchTypeAnnotation(typeAst->codeLoc, typeOpt.value());
            return false;
        }
        break;
    case NodeVal::Kind::kKnownVal:
        if (val.knownVal.type != typeOpt.value()) {
            if (orError) msgs->errorMismatchTypeAnnotation(typeAst->codeLoc, typeOpt.value());
            return false;
        }
        break;
    default:
        if (orError) msgs->errorMismatchTypeAnnotation(typeAst->codeLoc, typeOpt.value());
        return false;
    }

    return true;
}

bool CodeProcessor::checkIsEmptyTerminal(const AstNode *ast, bool orError) {
    if (!ast->isTerminal() || ast->terminal->kind != TerminalVal::Kind::kEmpty) {
        if (orError) msgs->errorUnknown(ast->codeLoc);
        return false;
    }

    return true;
}

bool CodeProcessor::checkIsEllipsis(const AstNode *ast, bool orError) {
    if (!ast->isTerminal() ||
        ast->terminal->kind != TerminalVal::Kind::kKeyword ||
        ast->terminal->keyword != Token::T_ELLIPSIS) {
        if (orError) msgs->errorUnknown(ast->codeLoc);
        return false;
    }

    return true;
}

bool CodeProcessor::checkIsTerminal(const AstNode *ast, bool orError) {
    if (!ast->isTerminal()) {
        if (orError) msgs->errorUnexpectedIsNotTerminal(ast->codeLoc);
        return false;
    }

    return true;
}

bool CodeProcessor::checkIsNotTerminal(const AstNode *ast, bool orError) {
    if (ast->isTerminal()) {
        if (orError) msgs->errorUnexpectedIsTerminal(ast->codeLoc);
        return false;
    }

    return true;
}

bool CodeProcessor::checkIsBlock(const AstNode *ast, bool orError) {
    if (ast->isTerminal() && ast->terminal->kind != TerminalVal::Kind::kEmpty) {
        if (orError) msgs->errorUnexpectedNotBlock(ast->codeLoc);
        return false;
    }

    return true;
}

bool CodeProcessor::checkExactlyChildren(const AstNode *ast, size_t n, bool orError) {
    if (ast->kind != AstNode::Kind::kTuple ||
        ast->children.size() != n) {
        if (orError) msgs->errorChildrenNotEq(ast->codeLoc, n);
        return false;
    }

    return true;
}

bool CodeProcessor::checkAtLeastChildren(const AstNode *ast, size_t n, bool orError) {
    if (ast->kind != AstNode::Kind::kTuple ||
        ast->children.size() < n) {
        if (orError) msgs->errorChildrenLessThan(ast->codeLoc, n);
        return false;
    }

    return true;
}

bool CodeProcessor::checkAtMostChildren(const AstNode *ast, size_t n, bool orError) {
    if (ast->kind != AstNode::Kind::kTuple ||
        ast->children.size() > n) {
        if (orError) msgs->errorChildrenMoreThan(ast->codeLoc, n);
        return false;
    }

    return true;
}

bool CodeProcessor::checkBetweenChildren(const AstNode *ast, std::size_t nLo, std::size_t nHi, bool orError) {
    if (ast->kind != AstNode::Kind::kTuple ||
        !between(ast->children.size(), nLo, nHi)) {
        if (orError) msgs->errorChildrenNotBetween(ast->codeLoc, nLo, nHi);
        return false;
    }

    return true;
}

bool CodeProcessor::checkValueUnbroken(CodeLoc codeLoc, const NodeVal &val, bool orError) {
    if (val.valueBroken()) {
        // if NodeVal::kInvalid, an error was probably already reported somewhere
        if (orError && !val.isInvalid()) msgs->errorExprNotValue(codeLoc);
        return false;
    }

    return true;
}

bool CodeProcessor::checkIsId(CodeLoc codeLoc, const NodeVal &val, bool orError) {
    if (!val.isId()) {
        if (orError) msgs->errorUnexpectedNotId(codeLoc);
        return false;
    }

    return true;
}

bool CodeProcessor::checkIsKeyword(CodeLoc codeLoc, const NodeVal &val, bool orError) {
    if (!val.isKeyword()) {
        if (orError) msgs->errorUnexpectedNotKeyword(codeLoc);
        return false;
    }

    return true;
}

bool CodeProcessor::checkIsOper(CodeLoc codeLoc, const NodeVal &val, bool orError) {
    if (!val.isOper()) {
        if (orError) msgs->errorUnknown(codeLoc);
        return false;
    }

    return true;
}

bool CodeProcessor::checkIsType(CodeLoc codeLoc, const NodeVal &val, bool orError) {
    if (!val.isType()) {
        if (orError) msgs->errorUnexpectedNotType(codeLoc);
        return false;
    }

    return true;
}

bool CodeProcessor::checkIsKnown(CodeLoc codeLoc, const NodeVal &val, bool orError) {
    if (!val.isKnownVal()) {
        if (orError) msgs->errorExprNotBaked(codeLoc);
        return false;
    }

    return true;
}

bool CodeProcessor::checkIsAttribute(CodeLoc codeLoc, const NodeVal &val, bool orError) {
    if (!val.isAttribute()) {
        if (orError) msgs->errorUnexpectedNotAttribute(codeLoc);
        return false;
    }

    return true;
}

bool CodeProcessor::checkGlobalScope(CodeLoc codeLoc, bool orError) {
    if (!isGlobalScope()) {
        if (orError) msgs->errorNotGlobalScope(codeLoc);
        return false;
    }

    return true;
}

bool CodeProcessor::checkIsBool(CodeLoc codeLoc, const NodeVal &val, bool orError) {
    if (!checkValueUnbroken(codeLoc, val, orError)) return false;
    if (!(val.isKnownVal() && KnownVal::isB(val.knownVal, getTypeTable())) &&
        !(val.isLlvmVal() && getTypeTable()->worksAsTypeB(val.llvmVal.type))) {
        msgs->errorExprCannotImplicitCast(codeLoc, val.isKnownVal() ? val.knownVal.type : val.llvmVal.type, getPrimTypeId(TypeTable::P_BOOL));
        return false;
    }
    return true;
}

optional<NamePool::Id> CodeProcessor::getId(const AstNode *ast, bool orError) {
    unique_ptr<AstNode> esc = ast->clone();
    esc->escaped = true;
    
    NodeVal nodeVal = processEvalNode(esc.get());
    if (!checkIsId(esc->codeLoc, nodeVal, orError)) return nullopt;

    return nodeVal.id;
}

optional<KnownVal> CodeProcessor::getKnownVal(const AstNode *ast, bool orError) {
    NodeVal nodeVal = processEvalNode(ast);
    if (!checkIsKnown(ast->codeLoc, nodeVal, orError)) return nullopt;

    return nodeVal.knownVal;
}

optional<TypeTable::Id> CodeProcessor::getType(const AstNode *ast, bool orError) {
    NodeVal type = processEvalNode(ast);
    if (!checkIsType(ast->codeLoc, type, true)) return nullopt;

    return type.type;
}

optional<CodeProcessor::NameTypePair> CodeProcessor::getIdTypePair(const AstNode *ast, bool orError) {
    optional<NamePool::Id> id = getId(ast, orError);
    if (!id.has_value()) return nullopt;

    if (!ast->type.has_value()) {
        if (orError) msgs->errorMissingTypeAnnotation(ast->codeLoc);
        return nullopt;
    }
    optional<TypeTable::Id> type = getType(ast->type.value().get(), orError);
    if (!type.has_value()) return nullopt;

    NameTypePair idType;
    idType.first = id.value();
    idType.second = type.value();
    return idType;
}

optional<Token::Attr> CodeProcessor::getAttr(const AstNode *ast, bool orError) {
    NodeVal nodeVal = processEvalNode(ast);
    if (!checkIsAttribute(ast->codeLoc, nodeVal, orError)) return nullopt;

    return nodeVal.attribute;
}