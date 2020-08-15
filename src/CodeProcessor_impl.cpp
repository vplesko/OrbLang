#include "CodeProcessor.h"
using namespace std;

NodeVal CodeProcessor::processTerminal(const AstNode *ast) {
    const TerminalVal &term = ast->terminal.value();

    NodeVal ret;

    switch (term.kind) {
    case TerminalVal::Kind::kKeyword:
        ret = NodeVal(NodeVal::Kind::kKeyword);
        ret.keyword = term.keyword;
        break;
    case TerminalVal::Kind::kOper:
        ret = NodeVal(NodeVal::Kind::kOper);
        ret.oper = term.oper;
        break;
    case TerminalVal::Kind::kId:
        ret = NodeVal(NodeVal::Kind::kId);
        ret.id = term.id;
        break;
    case TerminalVal::Kind::kAttribute:
        ret = NodeVal(NodeVal::Kind::kAttribute);
        ret.attribute = term.attribute;
        break;
    case TerminalVal::Kind::kVal:
        ret = processKnownVal(ast);
        break;
    case TerminalVal::Kind::kEmpty:
        ret = NodeVal(NodeVal::Kind::kEmpty);
        break;
    };

    return ret;
}

NodeVal CodeProcessor::processNode(const AstNode *ast) {
    NodeVal ret;

    if (ast->kind == AstNode::Kind::kTerminal) {
        ret = processTerminal(ast);

        if (ret.kind == NodeVal::Kind::kId && !ast->escaped) {
            const TerminalVal &term = ast->terminal.value();

            if (getTypeTable()->isType(term.id)) {
                TypeTable::Id type = getTypeTable()->getTypeId(term.id).value();
                ret = NodeVal(NodeVal::Kind::kType);
                ret.type = type;
            } else if (symbolTable->isVarName(term.id)) {
                ret = processVar(ast);
            } else if (symbolTable->isFuncName(term.id)) {
                ret = NodeVal(NodeVal::Kind::kFuncId);
                ret.id = term.id;
            } else if (symbolTable->isMacroName(term.id)) {
                ret = NodeVal(NodeVal::Kind::kMacroId);
                ret.id = term.id;
            } else {
                msgs->errorVarNotFound(ast->codeLoc, term.id);
                return NodeVal();
            }
        }
    } else {
        NodeVal starting = processNode(ast->children[0].get());
        if (starting.kind == NodeVal::Kind::kInvalid) return NodeVal();

        if (starting.isMacroId()) {
            unique_ptr<AstNode> expanded = processInvoke(starting.id, ast);
            if (expanded == nullptr) return NodeVal();
            return processNode(expanded.get());
        }

        if (starting.isType()) {
            ret = processType(ast, starting);
        } else if (starting.isKeyword()) {
            switch (starting.keyword) {
            case Token::T_SYM:
                ret = processSym(ast);
                break;
            case Token::T_BLOCK:
                ret = processBlock(ast);
                break;
            case Token::T_EXIT:
                ret = processExit(ast);
                break;
            case Token::T_PASS:
                ret = processPass(ast);
                break;
            case Token::T_LOOP:
                ret = processLoop(ast);
                break;
            case Token::T_RET:
                ret = processRet(ast);
                break;
            case Token::T_FNC:
                ret = processFunc(ast);
                break;
            case Token::T_MAC:
                // TODO think of something better
                ret = processMac(const_cast<AstNode*>(ast));
                break;
            case Token::T_CAST:
                ret = processCast(ast);
                break;
            case Token::T_ARR:
                ret = processArr(ast);
                break;
            case Token::T_IMPORT:
                ret = processImport(ast);
                break;
            default:
                msgs->errorUnexpectedKeyword(ast->children[0].get()->codeLoc, starting.keyword);
                return NodeVal();
            }
        } else if (starting.isType()) {
            ret = processType(ast, starting);
        } else {
            ret = processExpr(ast, starting);
        }
    }

    if (ret.isInvalid()) return NodeVal();

    if (!ast->escaped && ast->type.has_value() &&
        !verifyTypeAnnotationMatch(ret, ast->type.value().get(), true))
        return NodeVal();

    return ret;
}

NodeVal CodeProcessor::processAll(const AstNode *ast) {
    if (!checkIsBlock(ast, true)) return NodeVal();
    if (ast->type.has_value()) {
        msgs->errorMismatchTypeAnnotation(ast->type.value()->codeLoc);
        return NodeVal();
    }

    for (const std::unique_ptr<AstNode> &child : ast->children) {
        if (!checkIsEmptyTerminal(child.get(), false) && !checkIsNotTerminal(child.get(), true)) return NodeVal();
        
        processNode(child.get());
        if (isSkippingProcessing()) return NodeVal();
    }

    return NodeVal();
}

NodeVal CodeProcessor::processImport(const AstNode *ast) {
    if (!checkGlobalScope(ast->codeLoc, true) ||
        !checkExactlyChildren(ast, 2, true))
        return NodeVal();
    
    const AstNode *nodeFile = ast->children[1].get();

    NodeVal valFile = processNode(nodeFile);
    if (!valFile.isKnownVal() || !KnownVal::isStr(valFile.knownVal, getTypeTable())) {
        msgs->errorImportNotString(nodeFile->codeLoc);
        return NodeVal();
    }

    NodeVal ret(NodeVal::Kind::kImport);
    ret.file = valFile.knownVal.str;
    return ret;
}

NodeVal CodeProcessor::processExit(const AstNode *ast) {
    if (!checkBetweenChildren(ast, 2, 3, true)) {
        return NodeVal();
    }

    bool hasName = ast->children.size() == 3;

    const AstNode *nodeName = hasName ? ast->children[1].get() : nullptr;
    const AstNode *nodeCond = ast->children[hasName ? 2 : 1].get();

    optional<NamePool::Id> name;
    if (hasName) {
        name = getId(nodeName, true);
        if (!name.has_value()) return NodeVal();
    }

    NodeVal valCond = processNode(nodeCond);
    if (isSkippingProcessing() || !checkIsBool(nodeCond->codeLoc, valCond, true)) return NodeVal();

    SymbolTable::Block *targetBlock;
    if (hasName) targetBlock = getBlock(nodeName->codeLoc, name.value(), true);
    else targetBlock = symbolTable->getLastBlock();
    if (isGlobalScope() || targetBlock == nullptr) {
        msgs->errorExitNowhere(ast->codeLoc);
        return NodeVal();
    }

    if (targetBlock->type.has_value()) {
        msgs->errorExitPassingBlock(ast->codeLoc);
        return NodeVal();
    }

    handleExit(targetBlock, nodeCond->codeLoc, valCond);

    return NodeVal();
}

NodeVal CodeProcessor::processLoop(const AstNode *ast) {
    if (!checkBetweenChildren(ast, 2, 3, true)) {
        return NodeVal();
    }

    bool hasName = ast->children.size() == 3;

    const AstNode *nodeName = hasName ? ast->children[1].get() : nullptr;
    const AstNode *nodeCond = ast->children[hasName ? 2 : 1].get();

    optional<NamePool::Id> name;
    if (hasName) {
        name = getId(nodeName, true);
        if (!name.has_value()) return NodeVal();
    }

    NodeVal valCond = processNode(nodeCond);
    if (isSkippingProcessing() || !checkIsBool(nodeCond->codeLoc, valCond, true)) return NodeVal();

    SymbolTable::Block *targetBlock;
    if (hasName) targetBlock = getBlock(nodeName->codeLoc, name.value(), true);
    else targetBlock = symbolTable->getLastBlock();
    if (isGlobalScope() || targetBlock == nullptr) {
        msgs->errorLoopNowhere(ast->codeLoc);
        return NodeVal();
    }

    handleLoop(targetBlock, nodeCond->codeLoc, valCond);

    return NodeVal();
}

NodeVal CodeProcessor::processPass(const AstNode *ast) {
    if (!checkBetweenChildren(ast, 2, 3, true)) {
        return NodeVal();
    }

    bool hasName = ast->children.size() == 3;

    const AstNode *nodeName = hasName ? ast->children[1].get() : nullptr;
    const AstNode *nodeValue = ast->children[hasName ? 2 : 1].get();

    optional<NamePool::Id> name;
    if (hasName) {
        name = getId(nodeName, true);
        if (!name.has_value()) return NodeVal();
    }

    SymbolTable::Block *targetBlock;
    if (hasName) targetBlock = getBlock(nodeName->codeLoc, name.value(), true);
    else targetBlock = symbolTable->getLastBlock();
    if (isGlobalScope() || targetBlock == nullptr) {
        msgs->errorExitNowhere(ast->codeLoc);
        return NodeVal();
    }

    if (!targetBlock->type.has_value()) {
        msgs->errorPassNonPassingBlock(nodeValue->codeLoc);
        return NodeVal();
    }

    TypeTable::Id expectedType = targetBlock->type.value();

    NodeVal value = processNode(nodeValue);
    if (isSkippingProcessing() || !checkValueUnbroken(nodeValue->codeLoc, value, true)) return NodeVal();

    handlePass(targetBlock, nodeValue->codeLoc, value, expectedType);

    return NodeVal();
}

NodeVal CodeProcessor::processBlock(const AstNode *ast) {
    if (!checkBetweenChildren(ast, 2, 4, true)) {
        return NodeVal();
    }

    bool hasName = false, hasType = false;
    size_t indName, indType, indBody;
    if (ast->children.size() == 2) {
        indBody = 1;
    } else if (ast->children.size() == 3) {
        hasType = true;
        indType = 1;
        indBody = 2;
    } else {
        hasName = true;
        hasType = true;
        indName = 1;
        indType = 2;
        indBody = 3;
    }

    const AstNode *nodeName = hasName ? ast->children[indName].get() : nullptr;
    const AstNode *nodeType = hasType ? ast->children[indType].get() : nullptr;
    const AstNode *nodeBody = ast->children[indBody].get();

    optional<NamePool::Id> name;
    if (hasName) {
        name = getId(nodeName, true);
        if (!name.has_value()) return NodeVal();
    }

    optional<TypeTable::Id> type;
    if (hasType) {
        if (!checkIsEmptyTerminal(nodeType, false)) {
            type = getType(nodeType, true);
            if (!type.has_value()) return NodeVal();
        } else {
            hasType = false;
        }
    }

    return handleBlock(name, type, nodeBody);
}