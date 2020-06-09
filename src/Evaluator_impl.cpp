#include "Evaluator.h"
#include <vector>
#include "Codegen.h"
using namespace std;

Evaluator::Evaluator(StringPool *stringPool, SymbolTable *symbolTable, AstStorage *astStorage, CompileMessages *msgs)
    : stringPool(stringPool), symbolTable(symbolTable), astStorage(astStorage), msgs(msgs), codegen(nullptr) {
}

CompilerAction Evaluator::evaluateGlobalNode(AstNode *ast) {
    if (ast->isTerminal()) return CompilerAction(CompilerAction::Kind::kCodegen);

    AstNode *starting = ast->children[0].get();

    if (starting->isTerminal() &&
        starting->terminal->kind == TerminalVal::Kind::kKeyword) {
        if (starting->terminal->keyword == Token::T_MAC) {
            evaluateMac(ast);
            return CompilerAction(CompilerAction::Kind::kNone);
        } else if (starting->terminal->keyword == Token::T_IMPORT) {
            NodeVal val = evaluateImport(ast);

            CompilerAction importAction(CompilerAction::Kind::kImport);
            importAction.file = val.file;
            return importAction;
        }
    }

    return CompilerAction(CompilerAction::Kind::kCodegen);
}

NodeVal Evaluator::evaluateTerminal(const AstNode *ast) {
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
        ret = evaluateUntypedVal(ast);
        break;
    case TerminalVal::Kind::kEmpty:
        ret = NodeVal(NodeVal::Kind::kEmpty);
        break;
    };

    return ret;
}

NodeVal Evaluator::evaluateNode(const AstNode *ast) {
    NodeVal ret;

    if (ast->kind == AstNode::Kind::kTerminal) {
        ret = evaluateTerminal(ast);

        if (ret.kind == NodeVal::Kind::kId && !ast->escaped) {
            const TerminalVal &term = ast->terminal.value();

            if (getTypeTable()->isType(term.id)) {
                TypeTable::Id type = getTypeTable()->getTypeId(term.id).value();
                ret = NodeVal(NodeVal::Kind::kType);
                ret.type = type;
            } else if (symbolTable->isMacroName(term.id)) {
                ret = NodeVal(NodeVal::Kind::kMacroId);
                ret.id = term.id;
            } else {
                msgs->errorEvaluationNotSupported(ast->codeLoc);
                return NodeVal();
            }
        }
    } else {
        NodeVal starting = evaluateNode(ast->children[0].get());
        if (starting.kind == NodeVal::Kind::kInvalid) return NodeVal();

        if (starting.isMacroId()) {
            unique_ptr<AstNode> expanded = evaluateInvoke(starting.id, ast);
            if (expanded == nullptr) return NodeVal();
            return evaluateNode(expanded.get());
        }

        if (starting.isType()) {
            ret = evaluateType(ast, starting);
        } else {
            ret = evaluateExpr(ast, starting);
        }
    }

    if (!ast->escaped && ast->type.has_value() && !ret.isUntyVal()) {
        // TODO! add type verification here
        msgs->errorEvaluationNotSupported(ast->type.value()->codeLoc);
        return NodeVal();
    }

    return ret;
}

optional<NodeVal> Evaluator::evaluateTypeDescr(const AstNode *ast, const NodeVal &first) {
    if (ast->children.size() < 2) return nullopt;

    const AstNode *nodeChild = ast->children[1].get();

    NodeVal descr = evaluateNode(nodeChild);

    if (!descr.isKeyword() && !descr.isOper() && !descr.isUntyVal())
        return nullopt;
    
    TypeTable::TypeDescr typeDescr(first.type);
    for (size_t i = 1; i < ast->children.size(); ++i) {
        nodeChild = ast->children[i].get();

        if (i > 1) {
            descr = evaluateNode(nodeChild);
        }

        if (descr.isOper() && descr.oper == Token::O_MUL) {
            typeDescr.addDecor({TypeTable::TypeDescr::Decor::D_PTR});
        } else if (descr.isOper() && descr.oper == Token::O_IND) {
            typeDescr.addDecor({TypeTable::TypeDescr::Decor::D_ARR_PTR});
        } else if (descr.isUntyVal()) {
            optional<uint64_t> arrSize = UntypedVal::getValueNonNeg(descr.untyVal, getTypeTable());
            if (!arrSize.has_value() || arrSize.value() == 0) {
                if (arrSize.value() == 0) {
                    msgs->errorBadArraySize(nodeChild->codeLoc, arrSize.value());
                } else {
                    optional<int64_t> integ = UntypedVal::getValueI(descr.untyVal, getTypeTable());
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

NodeVal Evaluator::evaluateType(const AstNode *ast, const NodeVal &first) {
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

NodeVal Evaluator::evaluateMac(AstNode *ast) {
    if (!codegen->checkGlobalScope(ast->codeLoc, true) ||
        !codegen->checkExactlyChildren(ast, 4, true)) {
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
    if (!codegen->checkEmptyTerminal(nodeArgs, false)) {
        if (!codegen->checkNotTerminal(nodeArgs, true)) return NodeVal();

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

NodeVal Evaluator::evaluateImport(const AstNode *ast) {
    if (!codegen->checkGlobalScope(ast->codeLoc, true) ||
        !codegen->checkExactlyChildren(ast, 2, true))
        return NodeVal();
    
    const AstNode *nodeFile = ast->children[1].get();

    NodeVal valFile = evaluateNode(nodeFile);
    if (!valFile.isUntyVal() || !isStr(valFile.untyVal)) {
        msgs->errorImportNotString(nodeFile->codeLoc);
        return NodeVal();
    }

    NodeVal ret(NodeVal::Kind::kImport);
    ret.file = valFile.untyVal.str;
    return ret;
}

unique_ptr<AstNode> Evaluator::evaluateInvoke(NamePool::Id macroName, const AstNode *ast) {
    if (!codegen->checkNotTerminal(ast, true)) return nullptr;

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