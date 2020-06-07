#include "Evaluator.h"
#include <vector>
#include "Codegen.h"
using namespace std;

Evaluator::Evaluator(SymbolTable *symbolTable, AstStorage *astStorage, CompileMessages *msgs)
    : symbolTable(symbolTable), astStorage(astStorage), msgs(msgs), codegen(nullptr) {
}

optional<NamePool::Id> Evaluator::getId(const AstNode *ast, bool orError) {
    // TODO! evaluate recursively (look at Codegen::getId)

    if (!codegen->checkTerminal(ast, false) ||
        ast->terminal->kind != TerminalVal::Kind::kId) {
        if (orError) msgs->errorUnexpectedNotId(ast->codeLoc);
        return nullopt;
    }
    if (ast->hasType()) {
        if (orError) msgs->errorMismatchTypeAnnotation(ast->codeLoc);
        return nullopt;
    }
    return ast->terminal->id;
}

bool Evaluator::evaluateGlobalNode(AstNode *ast) {
    if (ast->isTerminal()) return false;

    AstNode *starting = ast->children[0].get();

    if (starting->isTerminal() &&
        starting->terminal->kind == TerminalVal::Kind::kKeyword &&
        starting->terminal->keyword == Token::T_MAC) {
        evaluateMac(ast);
        return true;
    }

    return false;
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

            if (symbolTable->isMacroName(term.id)) {
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
            unique_ptr<AstNode> expanded = evaluateInvoke(ast);
            if (expanded == nullptr) return NodeVal();
            return evaluateNode(expanded.get());
        }

        if (starting.isOper()) {
            if (ast->children.size() == 2) {
                return evaluateOperUnary(ast, starting);
            } else {
                return evaluateOper(ast, starting);
            }
        } else {
            msgs->errorEvaluationNotSupported(ast->children[0]->codeLoc);
            return NodeVal();
        }
    }

    if (!ast->escaped && ast->type.has_value()) {
        // TODO! replace codegenType with evaluateType, add type verification here
        msgs->errorEvaluationNotSupported(ast->type.value()->codeLoc);
        return NodeVal();
    }

    return ret;
}

void Evaluator::evaluateMac(AstNode *ast) {
    if (!codegen->checkGlobalScope(ast->codeLoc, true) ||
        !codegen->checkExactlyChildren(ast, 4, true)) {
        return;
    }

    const AstNode *nodeName = ast->children[1].get();
    const AstNode *nodeArgs = ast->children[2].get();
    unique_ptr<AstNode> &nodeBody = ast->children[3];

    optional<NamePool::Id> nameOpt = getId(nodeName, true);
    if (!nameOpt.has_value()) return;
    NamePool::Id name = nameOpt.value();

    if (!symbolTable->macroMayTakeName(name)) {
        msgs->errorMacroNameTaken(nodeName->codeLoc, name);
        return;
    }

    vector<NamePool::Id> args;
    if (!codegen->checkEmptyTerminal(nodeArgs, false)) {
        if (!codegen->checkNotTerminal(nodeArgs, true)) return;

        for (size_t i = 0; i < nodeArgs->children.size(); ++i) {
            const AstNode *nodeArg = nodeArgs->children[i].get();

            optional<NamePool::Id> nameOpt = getId(nodeArg, true);
            if (!nameOpt.has_value()) return;

            args.push_back(nameOpt.value());
        }
    }
    if (nodeArgs->hasType()) {
        msgs->errorMismatchTypeAnnotation(nodeArgs->codeLoc);
        return;
    }

    // can't have args with same name
    for (size_t i = 0; i+1 < args.size(); ++i) {
        for (size_t j = i+1; j < args.size(); ++j) {
            if (args[i] == args[j]) {
                msgs->errorArgNameDuplicate(ast->codeLoc, args[j]);
                return;
            }
        }
    }

    MacroValue val;
    val.name = name;
    val.argNames = move(args);
    val.body = nodeBody.get();

    if (nodeBody->hasType()) {
        msgs->errorMismatchTypeAnnotation(nodeBody->codeLoc);
        return;
    }

    if (!symbolTable->canRegisterMacro(val)) {
        msgs->errorSigConflict(ast->codeLoc);
        return;
    } else {
        symbolTable->registerMacro(val);
        astStorage->store(move(nodeBody));
    }
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

unique_ptr<AstNode> Evaluator::evaluateInvoke(const AstNode *ast) {
    if (!codegen->checkNotTerminal(ast, true)) return nullptr;

    const AstNode *nodeMacroName = ast->children[0].get();

    optional<NamePool::Id> nameOpt = getId(nodeMacroName, true);
    if (!nameOpt.has_value()) return nullptr;
    NamePool::Id name = nameOpt.value();

    MacroSignature sig;
    sig.name = name;
    sig.argCount = ast->children.size()-1;

    optional<MacroValue> macro = symbolTable->getMacro(sig);
    if (!macro.has_value()) {
        msgs->errorMacroNotFound(ast->codeLoc, name);
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