#include "Evaluator.h"
#include "Codegen.h"
using namespace std;

optional<NamePool::Id> Evaluator::getId(const AstNode *ast, bool orError) {
    unique_ptr<AstNode> esc = ast->clone();
    esc->escaped = true;
    
    NodeVal nodeVal = evaluateNode(esc.get());
    if (!codegen->checkIsId(esc->codeLoc, nodeVal, orError)) return nullopt;

    return nodeVal.id;
}

optional<Token::Type> Evaluator::getKeyword(const AstNode *ast, bool orError) {
    NodeVal nodeVal = evaluateNode(ast);
    if (!codegen->checkIsKeyword(ast->codeLoc, nodeVal, orError)) return nullopt;

    return nodeVal.keyword;
}

optional<Token::Oper> Evaluator::getOper(const AstNode *ast, bool orError) {
    NodeVal nodeVal = evaluateNode(ast);
    if (!codegen->checkIsOper(ast->codeLoc, nodeVal, orError)) return nullopt;

    return nodeVal.oper;
}

optional<UntypedVal> Evaluator::getUntypedVal(const AstNode *ast, bool orError) {
    NodeVal nodeVal = evaluateNode(ast);
    if (!codegen->checkIsUntyped(ast->codeLoc, nodeVal, orError)) return nullopt;

    return nodeVal.untyVal;
}

optional<TypeTable::Id> Evaluator::getType(const AstNode *ast, bool orError) {
    NodeVal type = evaluateNode(ast);
    if (!codegen->checkIsType(ast->codeLoc, type, true)) return nullopt;

    return type.type;
}