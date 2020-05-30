#include "Evaluator.h"
#include <vector>
#include "Codegen.h"
using namespace std;

Evaluator::Evaluator(SymbolTable *symbolTable, AstStorage *astStorage, CompileMessages *msgs)
    : symbolTable(symbolTable), astStorage(astStorage), msgs(msgs), codegen(nullptr) {
}

optional<NamePool::Id> Evaluator::getId(const AstNode *ast, bool orError) {
    // TODO evaluate recursively

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

bool Evaluator::evaluateNode(AstNode *ast) {
    // TODO should evaluate into NodeVal recursively
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