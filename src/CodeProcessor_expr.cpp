#include "CodeProcessor.h"
using namespace std;

NodeVal CodeProcessor::processExpr(const AstNode *ast, const NodeVal &first) {
    if (first.isFuncId()) {
        return processCall(ast, first);
    } else if (first.isOper()) {
        if (first.oper == Token::O_DOT) {
            return processOperDot(ast);
        } else if (first.oper == Token::O_IND) {
            return processOperInd(ast);
        } else if (ast->children.size() == 2) {
            return processOperUnary(ast, first);
        } else {
            return processOper(ast, first);
        }
    } else {
        return processTuple(ast, first);
    }
}

NodeVal CodeProcessor::processOper(const AstNode *ast, const NodeVal &first) {
    OperInfo opInfo = operInfos.at(first.oper);

    if (opInfo.variadic ?
        !checkAtLeastChildren(ast, 3, true) :
        !checkExactlyChildren(ast, 3, true)) {
        return NodeVal();
    }

    if (opInfo.l_assoc) {
        const AstNode *nodeLhs = ast->children[1].get();
        NodeVal lhsVal = processNode(nodeLhs);
        if (isSkippingProcessing()) return NodeVal();

        for (size_t i = 2; i < ast->children.size(); ++i) {
            const AstNode *nodeRhs = ast->children[i].get();
            NodeVal rhsVal = processNode(nodeRhs);
            if (isSkippingProcessing()) return NodeVal();

            lhsVal = handleOper(nodeRhs->codeLoc, first.oper, lhsVal, rhsVal);
            if (lhsVal.isInvalid()) return NodeVal();
        }

        return lhsVal;
    } else {
        const AstNode *nodeRhs = ast->children.back().get();
        NodeVal rhsVal = processNode(nodeRhs);

        for (size_t i = ast->children.size()-2;; --i) {
            const AstNode *nodeLhs = ast->children[i].get();
            NodeVal lhsVal = processNode(nodeLhs);
            if (isSkippingProcessing()) return NodeVal();

            rhsVal = handleOper(nodeLhs->codeLoc, first.oper, lhsVal, rhsVal);
            if (rhsVal.isInvalid()) return NodeVal();

            if (i == 1) break;
        }

        return rhsVal;
    }
}