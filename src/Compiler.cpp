#include "Compiler.h"
#include <unordered_map>
#include <stack>
#include <filesystem>
#include "SymbolTable.h"
#include "Lexer.h"
#include "Parser.h"
#include "ClangAdapter.h"
using namespace std;

Compiler::Compiler(ostream &out) {
    namePool = make_unique<NamePool>();
    stringPool = make_unique<StringPool>();
    typeTable = make_unique<TypeTable>();
    symbolTable = make_unique<SymbolTable>(stringPool.get(), typeTable.get());
    msgs = make_unique<CompileMessages>(namePool.get(), stringPool.get(), symbolTable.get(), out);
    codegen = make_unique<Codegen>(namePool.get(), stringPool.get(), symbolTable.get(), msgs.get());

    genPrimTypes();
}

void Compiler::genPrimTypes() {
    // b
    symbolTable->getTypeTable()->addPrimType(
        namePool->add("bool"),
        TypeTable::P_BOOL,
        codegen->genPrimTypeBool()
    );
    // i
    symbolTable->getTypeTable()->addPrimType(
        namePool->add("i8"),
        TypeTable::P_I8,
        codegen->genPrimTypeI(8)
    );
    symbolTable->getTypeTable()->addPrimType(
        namePool->add("i16"),
        TypeTable::P_I16,
        codegen->genPrimTypeI(16)
    );
    symbolTable->getTypeTable()->addPrimType(
        namePool->add("i32"),
        TypeTable::P_I32,
        codegen->genPrimTypeI(32)
    );
    symbolTable->getTypeTable()->addPrimType(
        namePool->add("i64"),
        TypeTable::P_I64,
        codegen->genPrimTypeI(64)
    );
    // u
    symbolTable->getTypeTable()->addPrimType(
        namePool->add("u8"),
        TypeTable::P_U8,
        codegen->genPrimTypeU(8)
    );
    symbolTable->getTypeTable()->addPrimType(
        namePool->add("u16"),
        TypeTable::P_U16,
        codegen->genPrimTypeU(16)
    );
    symbolTable->getTypeTable()->addPrimType(
        namePool->add("u32"),
        TypeTable::P_U32,
        codegen->genPrimTypeU(32)
    );
    symbolTable->getTypeTable()->addPrimType(
        namePool->add("u64"),
        TypeTable::P_U64,
        codegen->genPrimTypeU(64)
    );
    // f
    symbolTable->getTypeTable()->addPrimType(
        namePool->add("f16"),
        TypeTable::P_F16,
        codegen->genPrimTypeF16()
    );
    symbolTable->getTypeTable()->addPrimType(
        namePool->add("f32"),
        TypeTable::P_F32,
        codegen->genPrimTypeF32()
    );
    symbolTable->getTypeTable()->addPrimType(
        namePool->add("f64"),
        TypeTable::P_F64,
        codegen->genPrimTypeF64()
    );
    symbolTable->getTypeTable()->addPrimType(
        namePool->add("ptr"),
        TypeTable::P_PTR,
        codegen->genPrimTypePtr()
    );
    // c
    symbolTable->getTypeTable()->addPrimType(
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

string canonical(const string &file) {
    return filesystem::canonical(file).string();
}

// TODO error reports don't show file name on imported files
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

    Parser par(namePool.get(), stringPool.get(), symbolTable.get(), msgs.get());

    unordered_map<string, unique_ptr<Lexer>> lexers;
    stack<Lexer*> trace;

    for (const string &in : inputs) {
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

                unique_ptr<AstNode> node = par.parseNode();
                if (msgs->isAbort()) return false;
                
                NodeVal nodeVal = codegen->codegenNode(node.get());
                if (msgs->isAbort()) return false;

                if (nodeVal.kind == NodeVal::Kind::kImport) {
                    string path = canonical(stringPool->get(nodeVal.file));
                    ImportTransRes imres = followImport(path, par, namePool.get(), stringPool.get(), msgs.get(), lexers);
                    if (imres == ITR_FAIL) {
                        return false;
                    } else if (imres == ITR_CYCLICAL) {
                        msgs->errorImportCyclical(node->codeLoc, path);
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
        // TODO see if lld::elf::link can be used

        const static string tempObjName = isOsWindows ? "a.obj" : "a.o";
        // TODO doesn't get called on linker failure
        DeferredFallback delObjTemp([&] { remove(tempObjName.c_str()); });

        if (!codegen->binary(tempObjName)) return false;
        
        return buildExecutable(tempObjName, output);
    }
}