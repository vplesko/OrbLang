#include "Evaluator.h"
#include <vector>
#include "Codegen.h"
using namespace std;

Evaluator::Evaluator(SymbolTable *symbolTable, AstStorage *astStorage, CompileMessages *msgs)
    : symbolTable(symbolTable), astStorage(astStorage), msgs(msgs), codegen(nullptr) {
}

bool Evaluator::evaluateNode(AstNode *ast) {
    // TODO should evaluate into NodeVal recursively
    if (ast->kind == AstNode::Kind::kTerminal) return false;

    AstNode *starting = ast->children[0].get();

    if (starting->kind == AstNode::Kind::kTerminal &&
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

    // TODO make a function for this
    if (!codegen->checkTerminal(nodeName, false) ||
        nodeName->terminal->kind != TerminalVal::Kind::kId) {
        msgs->errorUnexpectedNotId(nodeName->codeLoc);
        return;
    }
    if (nodeName->hasType()) {
        msgs->errorMismatchTypeAnnotation(nodeName->codeLoc);
        return;
    }
    // TODO evaluate recursively
    NamePool::Id name = nodeName->terminal->id;

    if (!symbolTable->macroMayTakeName(name)) {
        msgs->errorMacroNameTaken(nodeName->codeLoc, name);
        return;
    }

    vector<NamePool::Id> args;
    if (!codegen->checkEmptyTerminal(nodeArgs, false)) {
        if (!codegen->checkNotTerminal(nodeArgs, true)) return;

        for (size_t i = 0; i < nodeArgs->children.size(); ++i) {
            const AstNode *nodeArg = nodeArgs->children[i].get();

            // TODO make a function for this
            if (!codegen->checkTerminal(nodeArg, false) ||
                nodeArg->terminal->kind != TerminalVal::Kind::kId) {
                msgs->errorUnexpectedNotId(nodeArg->codeLoc);
                return;
            }
            if (nodeArg->hasType()) {
                msgs->errorMismatchTypeAnnotation(nodeArg->codeLoc);
                return;
            }

            // TODO evaluate recursively
            args.push_back(nodeArg->terminal->id);
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