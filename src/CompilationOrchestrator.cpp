#include "CompilationOrchestrator.h"
#include <unordered_map>
#include <stack>
#include <filesystem>
#include "OrbCompilerConfig.h"
#include "Reserved.h"
#include "SymbolTable.h"
#include "Lexer.h"
#include "Parser.h"
#include "ClangAdapter.h"
using namespace std;

CompilationOrchestrator::CompilationOrchestrator(ostream &out) {
    namePool = make_unique<NamePool>();
    stringPool = make_unique<StringPool>();
    typeTable = make_unique<TypeTable>();
    symbolTable = make_unique<SymbolTable>();
    msgs = make_unique<CompilationMessages>(namePool.get(), stringPool.get(), typeTable.get(), symbolTable.get(), out);
    evaluator = make_unique<Evaluator>(namePool.get(), stringPool.get(), typeTable.get(), symbolTable.get(), msgs.get());
    compiler = make_unique<Compiler>(namePool.get(), stringPool.get(), typeTable.get(), symbolTable.get(), msgs.get());
    evaluator->setCompiler(compiler.get());
    compiler->setEvaluator(evaluator.get());

    genReserved();
    genPrimTypes();
}

static void addMain(NamePool *namePool) {
    NamePool::Id name = namePool->addMain("main");
    meaningfuls.insert(make_pair(name, Meaningful::MAIN));
}

static void addMeaningful(NamePool *namePool, const std::string &str, Meaningful m) {
    NamePool::Id name = namePool->add(str);
    meaningfuls.insert(make_pair(name, m));
}

static void addKeyword(NamePool *namePool, const std::string &str, Keyword k) {
    NamePool::Id name = namePool->add(str);
    keywords.insert(make_pair(name, k));
}

static void addOper(NamePool *namePool, const std::string &str, Oper o) {
    NamePool::Id name = namePool->add(str);
    opers.insert(make_pair(name, o));
}

void CompilationOrchestrator::genReserved() {
    addMain(namePool.get());
    addMeaningful(namePool.get(), "cn", Meaningful::CN);
    addMeaningful(namePool.get(), "*", Meaningful::ASTERISK);
    addMeaningful(namePool.get(), "[]", Meaningful::SQUARE);
    addMeaningful(namePool.get(), "type", Meaningful::TYPE);

    addKeyword(namePool.get(), "sym", Keyword::SYM);
    addKeyword(namePool.get(), "cast", Keyword::CAST);
    addKeyword(namePool.get(), "block", Keyword::BLOCK);
    addKeyword(namePool.get(), "exit", Keyword::EXIT);
    addKeyword(namePool.get(), "loop", Keyword::LOOP);
    addKeyword(namePool.get(), "pass", Keyword::PASS);
    addKeyword(namePool.get(), "explicit", Keyword::EXPLICIT);
    addKeyword(namePool.get(), "data", Keyword::DATA);
    addKeyword(namePool.get(), "fnc", Keyword::FNC);
    addKeyword(namePool.get(), "ret", Keyword::RET);
    addKeyword(namePool.get(), "mac", Keyword::MAC);
    addKeyword(namePool.get(), "eval", Keyword::EVAL);
    addKeyword(namePool.get(), "typeOf", Keyword::TYPE_OF);
    addKeyword(namePool.get(), "lenOf", Keyword::LEN_OF);
    addKeyword(namePool.get(), "sizeOf", Keyword::SIZE_OF);
    addKeyword(namePool.get(), "??", Keyword::IS_DEF);
    addKeyword(namePool.get(), "attrOf", Keyword::ATTR_OF);
    addKeyword(namePool.get(), "attr??", Keyword::ATTR_IS_DEF);
    addKeyword(namePool.get(), "isEval", Keyword::IS_EVAL);
    addKeyword(namePool.get(), "import", Keyword::IMPORT);
    addKeyword(namePool.get(), "message", Keyword::MESSAGE);

    addOper(namePool.get(), "+", Oper::ADD);
    addOper(namePool.get(), "-", Oper::SUB);
    addOper(namePool.get(), "*", Oper::MUL);
    addOper(namePool.get(), "/", Oper::DIV);
    addOper(namePool.get(), "%", Oper::REM);
    addOper(namePool.get(), "<<", Oper::SHL);
    addOper(namePool.get(), ">>", Oper::SHR);
    addOper(namePool.get(), "&", Oper::BIT_AND);
    addOper(namePool.get(), "^", Oper::BIT_XOR);
    addOper(namePool.get(), "|", Oper::BIT_OR);
    addOper(namePool.get(), "==", Oper::EQ);
    addOper(namePool.get(), "!=", Oper::NE);
    addOper(namePool.get(), "<", Oper::LT);
    addOper(namePool.get(), "<=", Oper::LE);
    addOper(namePool.get(), ">", Oper::GT);
    addOper(namePool.get(), ">=", Oper::GE);
    addOper(namePool.get(), "=", Oper::ASGN);
    addOper(namePool.get(), "!", Oper::NOT);
    addOper(namePool.get(), "~", Oper::BIT_NOT);
    addOper(namePool.get(), "[]", Oper::IND);
}

void CompilationOrchestrator::genPrimTypes() {
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
    // raw
    typeTable->addPrimType(
        namePool->add("raw"),
        TypeTable::P_RAW,
        nullptr
    );
    // b
    typeTable->addPrimType(
        namePool->add("bool"),
        TypeTable::P_BOOL,
        compiler->genPrimTypeBool()
    );
    // i
    typeTable->addPrimType(
        namePool->add("i8"),
        TypeTable::P_I8,
        compiler->genPrimTypeI(8)
    );
    typeTable->addPrimType(
        namePool->add("i16"),
        TypeTable::P_I16,
        compiler->genPrimTypeI(16)
    );
    typeTable->addPrimType(
        namePool->add("i32"),
        TypeTable::P_I32,
        compiler->genPrimTypeI(32)
    );
    typeTable->addPrimType(
        namePool->add("i64"),
        TypeTable::P_I64,
        compiler->genPrimTypeI(64)
    );
    // u
    typeTable->addPrimType(
        namePool->add("u8"),
        TypeTable::P_U8,
        compiler->genPrimTypeU(8)
    );
    typeTable->addPrimType(
        namePool->add("u16"),
        TypeTable::P_U16,
        compiler->genPrimTypeU(16)
    );
    typeTable->addPrimType(
        namePool->add("u32"),
        TypeTable::P_U32,
        compiler->genPrimTypeU(32)
    );
    typeTable->addPrimType(
        namePool->add("u64"),
        TypeTable::P_U64,
        compiler->genPrimTypeU(64)
    );
    // f
    typeTable->addPrimType(
        namePool->add("f32"),
        TypeTable::P_F32,
        compiler->genPrimTypeF32()
    );
    typeTable->addPrimType(
        namePool->add("f64"),
        TypeTable::P_F64,
        compiler->genPrimTypeF64()
    );
    typeTable->addPrimType(
        namePool->add("ptr"),
        TypeTable::P_PTR,
        compiler->genPrimTypePtr()
    );
    // c
    typeTable->addPrimType(
        namePool->add("c8"),
        TypeTable::P_C8,
        compiler->genPrimTypeC(8)
    );
}

enum ImportTransRes {
    ITR_STARTED,
    ITR_CYCLICAL,
    ITR_COMPLETED,
    ITR_FAIL
};

static optional<string> locateOrbFile(const string &file, const vector<string> &additionalImportPaths) {
    if (filesystem::exists(file)) return filesystem::canonical(file).string();

    for (const string &path : additionalImportPaths) {
        filesystem::path importFile = filesystem::path(path) / file;
        if (filesystem::exists(importFile)) return filesystem::canonical(importFile).string();
    }

    {
        filesystem::path libFile = filesystem::path(ORBC_LIBS_PATH) / file;
        if (filesystem::exists(libFile)) return filesystem::canonical(libFile).string();
    }

    return nullopt;
}

static ImportTransRes followImport(
    const string &path, Parser &par, NamePool *names, StringPool *strings, CompilationMessages *msgs,
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

bool CompilationOrchestrator::process(const ProgramArgs &args) {
    if (args.inputsSrc.empty()) return true;

    compiler->setArgs(args);

    Parser par(stringPool.get(), typeTable.get(), msgs.get());

    unordered_map<string, unique_ptr<Lexer>> lexers;
    stack<Lexer*> trace;

    for (const string &in : args.inputsSrc) {
        optional<string> pathOpt = locateOrbFile(in, args.importPaths);
        if (!pathOpt.has_value()) {
            msgs->errorInputFileNotFound(in);
            return false;
        }
        const string &path = pathOpt.value();

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

                NodeVal val = compiler->processNode(node, true);
                if (msgs->isFail()) return false;

                if (val.isImport()) {
                    const string &file = stringPool->get(val.getImportFile());
                    optional<string> pathOpt = locateOrbFile(file, args.importPaths);
                    if (!pathOpt.has_value()) {
                        msgs->errorImportNotFound(node.getCodeLoc(), file);
                        return false;
                    }
                    const string &path = pathOpt.value();

                    ImportTransRes imres = followImport(path, par, namePool.get(), stringPool.get(), msgs.get(), lexers);
                    if (imres == ITR_FAIL) {
                        return false;
                    } else if (imres == ITR_CYCLICAL) {
                        msgs->errorImportCyclical(node.getCodeLoc(), path);
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

void CompilationOrchestrator::printout(const std::string &filename) const {
    compiler->printout(filename);
}

bool CompilationOrchestrator::compile(const ProgramArgs &args) {
    if (!args.link) {
        return compiler->binary(args.outputBin);
    } else if (!args.inputsSrc.empty()) {
        if (!symbolTable->isFuncName(getMeaningfulNameId(Meaningful::MAIN))) {
            msgs->errorNoMain();
            return false;
        }
        if (!symbolTable->getFunc(symbolTable->getFuncIds(getMeaningfulNameId(Meaningful::MAIN)).front()).defined) {
            msgs->errorMainNoDef();
            return false;
        }

        const static string tempObjName = PLATFORM_WINDOWS ? "a.obj" : "a.o";

        if (!compiler->binary(tempObjName)) return false;

        bool success = buildExecutable(args, tempObjName);

        remove(tempObjName.c_str());
        return success;
    } else {
        return buildExecutable(args, "");
    }
}

bool CompilationOrchestrator::isInternalError() const {
    return msgs->getStatus() == CompilationMessages::S_INTERNAL;
}