#include "Compiler.h"
#include <fstream>
#include "SymbolTable.h"
#include "Lexer.h"
#include "Parser.h"
using namespace std;

Compiler::Compiler() {
    namePool = make_unique<NamePool>();
    typeTable = make_unique<TypeTable>();
    symbolTable = make_unique<SymbolTable>(typeTable.get());
    codegen = make_unique<Codegen>(namePool.get(), symbolTable.get());

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

bool Compiler::parse(const std::string &filename) {
    ifstream file(filename);
    if (!file.is_open()) return false;

    Lexer lex(namePool.get());
    lex.start(file);

    Parser par(namePool.get(), symbolTable.get(), &lex);
    while (!par.isOver()) {
        unique_ptr<BaseAst> node = par.parseNode();
        if (par.isPanic()) return false;
        codegen->codegenNode(node.get());
        if (codegen->isPanic()) return false;
    }

    return true;
}

void Compiler::printout() const {
    codegen->printout();
}

bool Compiler::compile(const std::string &filename) {
    return codegen->binary(filename);
}

Compiler::~Compiler() {
}