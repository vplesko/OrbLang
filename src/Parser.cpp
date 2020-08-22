#include "Parser.h"
#include <iostream>
#include <sstream>
#include <cstdint>
using namespace std;

Parser::Parser(StringPool *stringPool, CompileMessages *msgs) 
    : stringPool(stringPool), lex(nullptr), msgs(msgs) {
}

const Token& Parser::peek() const {
    return lex->peek();
}

Token Parser::next() {
    return lex->next();
}

// If the next token matches the type, eats it and returns true.
// Otherwise, returns false.
bool Parser::match(Token::Type type) {
    if (peek().type != type) return false;
    next();
    return true;
}

// If the next token matches the type, eats it and returns true.
// Otherwise, files an error and returns false.
bool Parser::matchOrError(Token::Type type) {
    if (!match(type)) {
        msgs->errorUnexpectedTokenType(loc(), type, peek());
        return false;
    }
    return true;
}

CodeLoc Parser::loc() const {
    return lex->loc();
}

NodeVal Parser::makeEmptyTerm() {
    return NodeVal(loc());
}

// cannot be semicolon-terminated
NodeVal Parser::parseType() {
    if (peek().type == Token::T_BRACE_L_REG || peek().type == Token::T_BRACE_L_CUR)
        return parseNode();
    else
        return parseTerm();
}

NodeVal Parser::parseTerm() {
    CodeLoc codeLocTok = loc();
    Token tok = next();

    LiteralVal val;
    switch (tok.type) {
    case Token::T_ID:
        val.kind = LiteralVal::Kind::kId;
        val.val_id = tok.nameId;
        break;
    case Token::T_NUM:
        val.kind = LiteralVal::Kind::kSint;
        val.val_si = tok.num;
        break;
    case Token::T_FNUM:
        val.kind = LiteralVal::Kind::kFloat;
        val.val_f = tok.fnum;
        term->terminal = TerminalVal(val);
        break;
    case Token::T_CHAR:
        val.kind = LiteralVal::Kind::kChar;
        val.val_c = tok.ch;
        break;
    case Token::T_BVAL:
        val.kind = LiteralVal::Kind::kBool;
        val.val_b = tok.bval;
        break;
    case Token::T_STRING:
        {
            stringstream ss;
            ss << stringPool->get(tok.stringId);
            while (peek().type == Token::T_STRING) {
                ss << stringPool->get(next().stringId);
            }
            val.kind = LiteralVal::Kind::kString;
            val.val_str = stringPool->add(ss.str());
            break;
        }
    case Token::T_NULL:
        val.kind = LiteralVal::Kind::kNull;
        break;    
    default:
        msgs->errorUnexpectedTokenType(codeLocTok, {Token::T_BRACE_L_REG, Token::T_BRACE_L_CUR}, tok);
        return NodeVal();
    }

    NodeVal ret(codeLocTok, val);

    if (match(Token::T_COLON)) {
        ret.typeAnnot = parseType();

        // type annotation on type annotation is not allowed (unless bracketed)
        if (ret.typeAnnot.isInvalid || ret.typeAnnot.typeAnnot.has_value()) return NodeVal();
    }

    return ret;
}

NodeVal Parser::parseNode() {
    if (lex == nullptr) {
        return nullptr;
    }

    CodeLoc codeLocNode = loc();
    NodeVal node;

    bool escaped = match(Token::T_BACKSLASH);

    if (peek().type == Token::T_BRACE_L_REG || peek().type == Token::T_BRACE_L_CUR) {
        Token openBrace = next();

        vector<NodeVal> children;
        node = NodeVal(codeLocNode, children);

        while (peek().type != Token::T_BRACE_R_REG && peek().type != Token::T_BRACE_R_CUR) {
            if (peek().type == Token::T_SEMICOLON) {
                next();

                NodeVal tuple = NodeVal(children[0]->codeLoc, children);
                node->children.push_back(tuple);
            } else {
                bool escaped = match(Token::T_BACKSLASH);
                NodeVal child;
                if (peek().type == Token::T_BRACE_L_REG || peek().type == Token::T_BRACE_L_CUR) {
                    child = parseNode();
                } else {
                    child = parseTerm();
                }
                if (child.isInvalid) return NodeVal();
                if (escaped) child.escape();
                children.push_back(child);
            }
        }
        
        CodeLoc codeLocCloseBrace = loc();
        Token closeBrace = next();

        if (openBrace.type == Token::T_BRACE_L_REG && closeBrace.type != Token::T_BRACE_R_REG) {
            msgs->errorUnexpectedTokenType(codeLocCloseBrace, Token::T_BRACE_R_REG, closeBrace);
            return nullptr;
        } else if (openBrace.type == Token::T_BRACE_L_CUR && closeBrace.type != Token::T_BRACE_R_CUR) {
            msgs->errorUnexpectedTokenType(codeLocCloseBrace, Token::T_BRACE_R_CUR, closeBrace);
            return nullptr;
        }

        for (size_t i = 0; i < children.size(); ++i) {
            node->children.push_back(move(children[i]));
        }
    } else {
        while (peek().type != Token::T_SEMICOLON) {
            if (!escaped) escaped = match(Token::T_BACKSLASH);
            NodeVal child;
            if (peek().type == Token::T_BRACE_L_REG || peek().type == Token::T_BRACE_L_CUR) {
                child = parseNode();
            } else {
                child = parseTerm();
            }
            if (child.isInvalid) return NodeVal();
            if (escaped) child.escape();
            escaped = false;
            node.getChildren().push_back(child);
        }
        next();
    }

    if (escaped) {
        node.escape();
        escaped = false;
    }

    if (node->children.empty()) {
        node = makeEmptyTerm();
    }

    if (match(Token::T_COLON)) {
        node.typeAnnot = parseType();

        // type annotation on type annotation is not allowed (unless bracketed)
        if (ret.typeAnnot.isInvalid || ret.typeAnnot.typeAnnot.has_value()) return NodeVal();
    }

    return node;
}