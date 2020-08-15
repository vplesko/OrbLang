#include "Evaluator.h"
#include <vector>
using namespace std;

optional<NodeVal> Evaluator::evaluateTypeDescr(const AstNode *ast, const NodeVal &first) {
    if (ast->children.size() < 2) return nullopt;

    const AstNode *nodeChild = ast->children[1].get();

    NodeVal descr = processNode(nodeChild);

    if (!descr.isKeyword() && !descr.isOper() && !descr.isKnownVal())
        return nullopt;
    
    TypeTable::TypeDescr typeDescr(first.type);
    for (size_t i = 1; i < ast->children.size(); ++i) {
        nodeChild = ast->children[i].get();

        if (i > 1) {
            descr = processNode(nodeChild);
        }

        if (descr.isOper() && descr.oper == Token::O_MUL) {
            typeDescr.addDecor({.type=TypeTable::TypeDescr::Decor::D_PTR});
        } else if (descr.isOper() && descr.oper == Token::O_IND) {
            typeDescr.addDecor({.type=TypeTable::TypeDescr::Decor::D_ARR_PTR});
        } else if (descr.isKnownVal()) {
            optional<uint64_t> arrSize = KnownVal::getValueNonNeg(descr.knownVal, getTypeTable());
            if (!arrSize.has_value() || arrSize.value() == 0) {
                if (arrSize.value() == 0) {
                    msgs->errorBadArraySize(nodeChild->codeLoc, arrSize.value());
                } else {
                    optional<int64_t> integ = KnownVal::getValueI(descr.knownVal, getTypeTable());
                    if (integ.has_value()) {
                        msgs->errorBadArraySize(nodeChild->codeLoc, integ.value());
                    } else {
                        msgs->errorInvalidTypeDecorator(nodeChild->codeLoc);
                    }
                }
                return NodeVal();
            }

            typeDescr.addDecor({TypeTable::TypeDescr::Decor::D_ARR, arrSize.value()});
        } else if (descr.isKeyword() && descr.keyword == Token::T_CN) {
            typeDescr.setLastCn();
        } else {
            msgs->errorInvalidTypeDecorator(nodeChild->codeLoc);
            return NodeVal();
        }
    }

    TypeTable::Id typeId = symbolTable->getTypeTable()->addTypeDescr(move(typeDescr));

    NodeVal ret(NodeVal::Kind::kType);
    ret.type = typeId;
    return ret;
}

NodeVal Evaluator::processType(const AstNode *ast, const NodeVal &first) {
    if (ast->children.size() == 1) {
        NodeVal ret(NodeVal::Kind::kType);
        ret.type = first.type;
        return ret;
    }
    
    optional<NodeVal> typeDescr = evaluateTypeDescr(ast, first);
    if (typeDescr.has_value()) return typeDescr.value();

    TypeTable::Tuple tup;
    tup.members.resize(ast->children.size());

    tup.members[0] = first.type;
    for (size_t i = 1; i < ast->children.size(); ++i) {
        const AstNode *nodeChild = ast->children[i].get();

        optional<TypeTable::Id> memb = getType(nodeChild, true);
        if (!memb.has_value()) return NodeVal();

        tup.members[i] = memb.value();
    }

    optional<TypeTable::Id> tupTypeId = getTypeTable()->addTuple(move(tup));
    if (!tupTypeId.has_value()) return NodeVal();

    NodeVal ret(NodeVal::Kind::kType);
    ret.type = tupTypeId.value();
    return ret;
}

NodeVal Evaluator::processMac(AstNode *ast) {
    if (!checkGlobalScope(ast->codeLoc, true) ||
        !checkExactlyChildren(ast, 4, true)) {
        return NodeVal();
    }

    const AstNode *nodeName = ast->children[1].get();
    const AstNode *nodeArgs = ast->children[2].get();
    unique_ptr<AstNode> &nodeBody = ast->children[3];

    optional<NamePool::Id> nameOpt = getId(nodeName, true);
    if (!nameOpt.has_value()) return NodeVal();
    NamePool::Id name = nameOpt.value();

    if (!symbolTable->macroMayTakeName(name)) {
        msgs->errorMacroNameTaken(nodeName->codeLoc, name);
        return NodeVal();
    }

    vector<NamePool::Id> args;
    if (!checkIsEmptyTerminal(nodeArgs, false)) {
        if (!checkIsNotTerminal(nodeArgs, true)) return NodeVal();

        for (size_t i = 0; i < nodeArgs->children.size(); ++i) {
            const AstNode *nodeArg = nodeArgs->children[i].get();

            optional<NamePool::Id> nameOpt = getId(nodeArg, true);
            if (!nameOpt.has_value()) return NodeVal();

            args.push_back(nameOpt.value());
        }
    }
    if (nodeArgs->hasType()) {
        msgs->errorMismatchTypeAnnotation(nodeArgs->codeLoc);
        return NodeVal();
    }

    // can't have args with same name
    for (size_t i = 0; i+1 < args.size(); ++i) {
        for (size_t j = i+1; j < args.size(); ++j) {
            if (args[i] == args[j]) {
                msgs->errorArgNameDuplicate(ast->codeLoc, args[j]);
                return NodeVal();
            }
        }
    }

    MacroValue val;
    val.name = name;
    val.argNames = move(args);
    val.body = nodeBody.get();

    if (nodeBody->hasType()) {
        msgs->errorMismatchTypeAnnotation(nodeBody->codeLoc);
        return NodeVal();
    }

    if (!symbolTable->canRegisterMacro(val)) {
        msgs->errorSigConflict(ast->codeLoc);
        return NodeVal();
    } else {
        symbolTable->registerMacro(val);
        astStorage->store(move(nodeBody));
    }

    return NodeVal();
}

void Evaluator::substitute(unique_ptr<AstNode> &body, const vector<NamePool::Id> &names, const vector<const AstNode*> &values) {
    if (body->isTerminal()) {
        if (body->terminal->kind == TerminalVal::Kind::kId) {
            for (size_t i = 0; i < names.size(); ++i) {
                if (body->terminal->id == names[i]) {
                    body = values[i]->clone();
                    break;
                }
            }
        }
    } else {
        for (auto &it : body->children) substitute(it, names, values);
    }
    
    if (body->hasType()) substitute(body->type.value(), names, values);
}

unique_ptr<AstNode> Evaluator::processInvoke(NamePool::Id macroName, const AstNode *ast) {
    if (!checkIsNotTerminal(ast, true)) return nullptr;

    MacroSignature sig;
    sig.name = macroName;
    sig.argCount = ast->children.size()-1;

    optional<MacroValue> macro = symbolTable->getMacro(sig);
    if (!macro.has_value()) {
        msgs->errorMacroNotFound(ast->codeLoc, macroName);
        return nullptr;
    }

    vector<const AstNode*> values(sig.argCount);
    for (size_t i = 0; i < sig.argCount; ++i)
        values[i] = ast->children[i+1].get();
    
    unique_ptr<AstNode> expanded = macro->body->clone();
    substitute(expanded, macro.value().argNames, values);
    if (ast->escaped) expanded->escaped = true;
    return expanded;
}

void Evaluator::handleExit(const SymbolTable::Block *targetBlock, CodeLoc condCodeLoc, NodeVal cond) {
    if (cond.knownVal.b) {
        exitIssued = true;
        if (targetBlock->name.has_value()) blockGoto = targetBlock->name.value();
    }
}

void Evaluator::handleLoop(const SymbolTable::Block *targetBlock, CodeLoc condCodeLoc, NodeVal cond) {
    if (cond.knownVal.b) {
        loopIssued = true;
        if (targetBlock->name.has_value()) blockGoto = targetBlock->name.value();
    }
}

void Evaluator::handlePass(const SymbolTable::Block *targetBlock, CodeLoc valCodeLoc, NodeVal val, TypeTable::Id expectedType) {
    if (!checkIsKnown(valCodeLoc, val, true)) return;

    if (!getTypeTable()->isImplicitCastable(val.knownVal.type, expectedType)) {
        msgs->errorExprCannotImplicitCast(valCodeLoc, val.knownVal.type, expectedType);
        return;
    }
    if (!cast(val.knownVal, expectedType)) {
        msgs->errorInternal(valCodeLoc);
        return;
    }

    blockPassVal = val;
    if (targetBlock->name.has_value()) blockGoto = targetBlock->name.value();
}

NodeVal Evaluator::handleBlock(std::optional<NamePool::Id> name, std::optional<TypeTable::Id> type, const AstNode *nodeBody) {
    SymbolTable::Block blockOpen;
    if (name.has_value()) blockOpen.name = name;
    if (type.has_value()) {
        blockOpen.type = type.value();
    }

    do {
        resetGotoIssuing();

        BlockControl blockCtrl(symbolTable, blockOpen);
        processAll(nodeBody);
    } while (loopIssued && (!blockGoto.has_value() || blockGoto == name));

    if (isGotoIssued() && (!blockGoto.has_value() || blockGoto == name)) {
        if (blockPassVal.has_value()) {
            NodeVal ret = blockPassVal.value();
            resetGotoIssuing();
            return ret;
        } else {
            resetGotoIssuing();
        }
    }

    if (!isGotoIssued() && type.has_value()) {
        msgs->errorBlockNoPass(nodeBody->codeLoc);
        return NodeVal();
    }

    return NodeVal();
}