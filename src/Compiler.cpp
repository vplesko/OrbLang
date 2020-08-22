#include "Compiler.h"
#include <unordered_map>
#include <stack>
#include <filesystem>
#include "Reserved.h"
#include "SymbolTable.h"
#include "Lexer.h"
#include "Parser.h"
#include "ClangAdapter.h"
using namespace std;

Compiler::Compiler(ostream &out) {
    namePool = make_unique<NamePool>();
    stringPool = make_unique<StringPool>();
    typeTable = make_unique<TypeTable>();
    symbolTable = make_unique<SymbolTable>();
    msgs = make_unique<CompileMessages>(namePool.get(), stringPool.get(), typeTable.get(), symbolTable.get(), out);
    evaluator = make_unique<Evaluator>(namePool.get(), stringPool.get(), typeTable.get(), symbolTable.get(), msgs.get());
    codegen = make_unique<Codegen>(evaluator.get(), namePool.get(), stringPool.get(), typeTable.get(), symbolTable.get(), msgs.get());

    genReserved();
    genPrimTypes();
}

#define ADD_MEANINGFUL(str, enum) { \
    NamePool::Id name = namePool->add(str); \
    meaningfuls.insert(make_pair(name, Meaningful::enum)); \
    namePool->addMeaningful(name); \
    }

#define ADD_KEYWORD(str, enum) { \
    NamePool::Id name = namePool->add(str); \
    keywords.insert(make_pair(name, Keyword::enum)); \
    namePool->addKeyword(name); \
    }

#define ADD_OPER(str, enum) { \
    NamePool::Id name = namePool->add(str); \
    opers.insert(make_pair(name, Oper::enum)); \
    namePool->addOper(name); \
    }

void Compiler::genReserved() {
    ADD_MEANINGFUL("main", MAIN);
    ADD_MEANINGFUL("cn", CN);
    ADD_MEANINGFUL("*", ASTERISK);
    ADD_MEANINGFUL("[]", SQUARE);
    ADD_MEANINGFUL("...", ELLIPSIS);

    ADD_KEYWORD("sym", SYM);
    ADD_KEYWORD("cast", CAST);
    ADD_KEYWORD("block", BLOCK);
    ADD_KEYWORD("exit", EXIT);
    ADD_KEYWORD("loop", LOOP);
    ADD_KEYWORD("pass", PASS);
    ADD_KEYWORD("fnc", FNC);
    ADD_KEYWORD("ret", RET);
    ADD_KEYWORD("mac", MAC);
    ADD_KEYWORD("eval", EVAL);
    ADD_KEYWORD("import", IMPORT);

    ADD_OPER("+", ADD);
    ADD_OPER("-", SUB);
    ADD_OPER("*", MUL);
    ADD_OPER("/", DIV);
    ADD_OPER("%", REM);
    ADD_OPER("<<", SHL);
    ADD_OPER(">>", SHR);
    ADD_OPER("&", BIT_AND);
    ADD_OPER("^", BIT_XOR);
    ADD_OPER("|", BIT_OR);
    ADD_OPER("==", EQ);
    ADD_OPER("!=", NEQ);
    ADD_OPER("<", LT);
    ADD_OPER("<=", LTEQ);
    ADD_OPER(">", GT);
    ADD_OPER(">=", GTEQ);
    ADD_OPER("=", ASGN);
    ADD_OPER("!", NOT);
    ADD_OPER("~", BIT_NOT);
    ADD_OPER("[]", IND);
    ADD_OPER(".", DOT);
}

void Compiler::genPrimTypes() {
    // id
    typeTable->addPrimType(
        namePool->add("id"),
        TypeTable::P_ID,
        nullptr
    );
    // type
    typeTable->addPrimType(
        namePool->add("type"),
        TypeTable::P_TYPE,
        nullptr
    );
    // b
    typeTable->addPrimType(
        namePool->add("bool"),
        TypeTable::P_BOOL,
        codegen->genPrimTypeBool()
    );
    // i
    typeTable->addPrimType(
        namePool->add("i8"),
        TypeTable::P_I8,
        codegen->genPrimTypeI(8)
    );
    typeTable->addPrimType(
        namePool->add("i16"),
        TypeTable::P_I16,
        codegen->genPrimTypeI(16)
    );
    typeTable->addPrimType(
        namePool->add("i32"),
        TypeTable::P_I32,
        codegen->genPrimTypeI(32)
    );
    typeTable->addPrimType(
        namePool->add("i64"),
        TypeTable::P_I64,
        codegen->genPrimTypeI(64)
    );
    // u
    typeTable->addPrimType(
        namePool->add("u8"),
        TypeTable::P_U8,
        codegen->genPrimTypeU(8)
    );
    typeTable->addPrimType(
        namePool->add("u16"),
        TypeTable::P_U16,
        codegen->genPrimTypeU(16)
    );
    typeTable->addPrimType(
        namePool->add("u32"),
        TypeTable::P_U32,
        codegen->genPrimTypeU(32)
    );
    typeTable->addPrimType(
        namePool->add("u64"),
        TypeTable::P_U64,
        codegen->genPrimTypeU(64)
    );
    // f
    typeTable->addPrimType(
        namePool->add("f32"),
        TypeTable::P_F32,
        codegen->genPrimTypeF32()
    );
    typeTable->addPrimType(
        namePool->add("f64"),
        TypeTable::P_F64,
        codegen->genPrimTypeF64()
    );
    typeTable->addPrimType(
        namePool->add("ptr"),
        TypeTable::P_PTR,
        codegen->genPrimTypePtr()
    );
    // c
    typeTable->addPrimType(
        namePool->add("c8"),
        TypeTable::P_C8,
        codegen->genPrimTypeC(8)
    );
}

enum ImportTransRes {
    ITR_STARTED,
    ITR_CYCLICAL,
    ITR_COMPLETED,
    ITR_FAIL
};

bool exists(const string &file) {
    return filesystem::exists(file);
}

string canonical(const string &file) {
    return filesystem::canonical(file).string();
}

ImportTransRes followImport(
    const string &path, Parser &par, NamePool *names, StringPool *strings, CompileMessages *msgs,
    unordered_map<string, unique_ptr<Lexer>> &lexers) {
    auto loc = lexers.find(path);
    if (loc == lexers.end()) {
        unique_ptr<Lexer> lex = make_unique<Lexer>(names, strings, msgs, path);
        if (!lex->start()) return ITR_FAIL;

        par.setLexer(lex.get());
        lexers.insert(make_pair(path, move(lex)));
        return ITR_STARTED;
    } else {
        par.setLexer(loc->second.get());

        if (par.isOver()) return ITR_COMPLETED;
        else return ITR_CYCLICAL;
    }
}

bool Compiler::parse(const vector<string> &inputs) {
    if (inputs.empty()) return false;

    Parser par(stringPool.get(), msgs.get());

    unordered_map<string, unique_ptr<Lexer>> lexers;
    stack<Lexer*> trace;

    for (const string &in : inputs) {
        if (!exists(in)) {
            msgs->errorInputFileNotFound(in);
            return false;
        }
        string path = canonical(in);
        ImportTransRes imres = followImport(path, par, namePool.get(), stringPool.get(), msgs.get(), lexers);
        if (imres == ITR_CYCLICAL || imres == ITR_FAIL) {
            // cyclical should logically not happen here
            return false;
        } else if (imres == ITR_COMPLETED) {
            continue;
        } else {
            trace.push(par.getLexer());
        }

        while (!trace.empty()) {
            par.setLexer(trace.top());

            while (true) {
                if (par.isOver()) {
                    trace.pop();
                    break;
                }

                NodeVal node = par.parseNode();
                if (msgs->isFail()) return false;

                NodeVal val = codegen->processNode(node);
                if (msgs->isFail()) return false;

                if (val.importFile.has_value()) {
                    const string &file = stringPool->get(val.importFile.value());
                    if (!exists(file)) {
                        msgs->errorImportNotFound(node.codeLoc, file);
                        return false;
                    }
                    string path = canonical(file);
                    ImportTransRes imres = followImport(path, par, namePool.get(), stringPool.get(), msgs.get(), lexers);
                    if (imres == ITR_FAIL) {
                        return false;
                    } else if (imres == ITR_CYCLICAL) {
                        msgs->errorImportCyclical(node.codeLoc, path);
                        return false;
                    }
                        
                    if (imres == ITR_STARTED) {
                        trace.push(par.getLexer());
                    }
                    break;
                }
            }
        }
    }

    return true;
}

void Compiler::printout() const {
    codegen->printout();
}

bool Compiler::compile(const std::string &output, bool exe) {
    if (!exe) {
        return codegen->binary(output);
    } else {
        const static string tempObjName = isOsWindows ? "a.obj" : "a.o";
        // TODO doesn't get called on linker failure
        DeferredFallback delObjTemp([&] { remove(tempObjName.c_str()); });

        if (!codegen->binary(tempObjName)) return false;
        
        return buildExecutable(tempObjName, output);
    }
}