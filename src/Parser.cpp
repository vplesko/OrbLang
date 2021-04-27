#include "Parser.h"
#include <iostream>
#include <sstream>
#include <cstdint>
using namespace std;

Parser::Parser(StringPool *stringPool, TypeTable *typeTable, CompilationMessages *msgs) 
    : stringPool(stringPool), typeTable(typeTable), lex(nullptr), msgs(msgs) {
}

const Token& Parser::peek() const {
    return lex->peek();
}

pair<CodeLoc, Token> Parser::next() {
    CodeLoc codeLoc;
    codeLoc.file = lex->file();
    codeLoc.start = lex->loc();
    Token tok = lex->next();
    codeLoc.end = lex->loc();

    return {codeLoc, tok};
}

// If the next token matches the type, eats it and returns true.
// Otherwise, returns false.
bool Parser::match(Token::Type type) {
    if (peek().type != type) return false;
    next();
    return true;
}

bool Parser::matchCloseBraceOrError(Token openBrace) {
    pair<CodeLoc, Token> closeBrace = next();

    if (openBrace.type == Token::T_BRACE_L_REG && closeBrace.second.type != Token::T_BRACE_R_REG) {
        msgs->errorUnexpectedTokenType(closeBrace.first, Token::T_BRACE_R_REG, closeBrace.second);
        return false;
    } else if (openBrace.type == Token::T_BRACE_L_CUR && closeBrace.second.type != Token::T_BRACE_R_CUR) {
        msgs->errorUnexpectedTokenType(closeBrace.first, Token::T_BRACE_R_CUR, closeBrace.second);
        return false;
    }

    return true;
}

EscapeScore Parser::parseEscapeScore() {
    EscapeScore escapeScore = 0;
    while (peek().type == Token::T_BACKSLASH || peek().type == Token::T_COMMA) {
        if (match(Token::T_BACKSLASH)) {
            ++escapeScore;
        } else {
            match(Token::T_COMMA);
            --escapeScore;
        }
    }
    return escapeScore;
}

void Parser::parseTypeAttr(NodeVal &node) {
    if (match(Token::T_COLON)) node.setTypeAttr(parseBare());
}

void Parser::parseNonTypeAttrs(NodeVal &node) {
    if (match(Token::T_DOUBLE_COLON)) {
        node.setNonTypeAttrs(parseBare());
    } else {
        if (peek().type == Token::T_COLON) {
            pair<CodeLoc, Token> colon = next();

            msgs->errorUnexpectedTokenType(colon.first, colon.second);
            msgs->hintAttrDoubleColon();
        }
    }
}

NodeVal Parser::parseBare() {
    EscapeScore escapeScore = parseEscapeScore();

    NodeVal nodeVal;
    if (peek().type == Token::T_BRACE_L_REG || peek().type == Token::T_BRACE_L_CUR)
        nodeVal = parseNode(true);
    else
        nodeVal = parseTerm(true);

    NodeVal::escape(nodeVal, typeTable, escapeScore);
    return nodeVal;
}

NodeVal Parser::parseTerm(bool ignoreAttrs) {
    pair<CodeLoc, Token> tok = next();

    LiteralVal val;
    switch (tok.second.type) {
    case Token::T_ID:
        val.kind = LiteralVal::Kind::kId;
        val.val_id = tok.second.nameId;
        break;
    case Token::T_NUM:
        val.kind = LiteralVal::Kind::kSint;
        val.val_si = tok.second.num;
        break;
    case Token::T_FNUM:
        val.kind = LiteralVal::Kind::kFloat;
        val.val_f = tok.second.fnum;
        break;
    case Token::T_CHAR:
        val.kind = LiteralVal::Kind::kChar;
        val.val_c = tok.second.ch;
        break;
    case Token::T_BVAL:
        val.kind = LiteralVal::Kind::kBool;
        val.val_b = tok.second.bval;
        break;
    case Token::T_STRING:
        val.kind = LiteralVal::Kind::kString;
        val.val_str = tok.second.stringId;
        break;
    case Token::T_NULL:
        val.kind = LiteralVal::Kind::kNull;
        break;
    default:
        msgs->errorUnexpectedTokenType(tok.first, tok.second);
        return NodeVal();
    }

    NodeVal ret(tok.first, val);

    if (!ignoreAttrs) {
        parseTypeAttr(ret);
        parseNonTypeAttrs(ret);
    }

    return ret;
}

NodeVal Parser::parseNode(bool ignoreAttrs) {
    CodeLoc codeLoc;
    codeLoc.file = lex->file();
    codeLoc.start = lex->loc();

    NodeVal node = NodeVal::makeEmpty(CodeLoc(), typeTable);

    EscapeScore escapeScore = parseEscapeScore();

    if (peek().type == Token::T_BRACE_L_REG || peek().type == Token::T_BRACE_L_CUR) {
        pair<CodeLoc, Token> openBrace = next();

        vector<NodeVal> children;

        while (peek().type != Token::T_BRACE_R_REG && peek().type != Token::T_BRACE_R_CUR) {
            if (peek().type == Token::T_SEMICOLON) {
                pair<CodeLoc, Token> semicolon = next();

                if (children.empty()) {
                    NodeVal::addChild(node, NodeVal::makeEmpty(semicolon.first, typeTable), typeTable);
                } else {
                    CodeLoc codeLoc;
                    codeLoc.file = lex->file();
                    codeLoc.start = children.front().getCodeLoc().start;
                    codeLoc.end = semicolon.first.end;

                    NodeVal tuple = NodeVal::makeEmpty(codeLoc, typeTable);
                    NodeVal::addChildren(tuple, move(children), typeTable); // children is emptied here
                    NodeVal::addChild(node, move(tuple), typeTable);
                }
            } else {
                EscapeScore escapeScore = parseEscapeScore();
                NodeVal child;
                if (peek().type == Token::T_BRACE_L_REG || peek().type == Token::T_BRACE_L_CUR) {
                    child = parseNode();
                } else {
                    child = parseTerm();
                }
                if (child.isInvalid()) return NodeVal();
                NodeVal::escape(child, typeTable, escapeScore);
                children.push_back(move(child));
            }
        }

        if (!matchCloseBraceOrError(openBrace.second)) return NodeVal();

        codeLoc.end = lex->loc(); // code loc point after close brace
        node.setCodeLoc(codeLoc);

        NodeVal::addChildren(node, move(children), typeTable);
    } else {
        while (peek().type != Token::T_SEMICOLON) {
            escapeScore += parseEscapeScore();
            NodeVal child;
            if (peek().type == Token::T_BRACE_L_REG || peek().type == Token::T_BRACE_L_CUR) {
                child = parseNode();
            } else {
                child = parseTerm();
            }
            if (child.isInvalid()) return NodeVal();
            NodeVal::escape(child, typeTable, escapeScore);
            escapeScore = 0;
            NodeVal::addChild(node, move(child), typeTable);
        }

        next();

        codeLoc.end = lex->loc(); // code loc point after semicolon
        node.setCodeLoc(codeLoc);
    }

    if (!ignoreAttrs) {
        parseTypeAttr(node);
        parseNonTypeAttrs(node);
    }

    NodeVal::escape(node, typeTable, escapeScore);

    return node;
}