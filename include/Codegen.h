#pragma once

#include "Processor.h"
#include "Evaluator.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

class Codegen : public Processor {
    Evaluator *evaluator;

    // TODO!
    NodeVal loadSymbol(NamePool::Id id) { return NodeVal(); }
    NodeVal cast(const NodeVal &node, TypeTable::Id ty) { return NodeVal(); }
    NodeVal evaluateNode(const NodeVal &node) { return NodeVal(); }
    bool makeFunction(const NodeVal &node, FuncValue &func) { return false; }

public:
    Codegen(Evaluator *evaluator, NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompileMessages *msgs);

    // TODO!
    llvm::Type* genPrimTypeBool() { return false; }
    llvm::Type* genPrimTypeI(unsigned bits) { return false; }
    llvm::Type* genPrimTypeU(unsigned bits) { return false; }
    llvm::Type* genPrimTypeC(unsigned bits) { return false; }
    llvm::Type* genPrimTypeF32() { return false; }
    llvm::Type* genPrimTypeF64() { return false; }
    llvm::Type* genPrimTypePtr() { return false; }

    // TODO!
    void printout() const {}
    bool binary(const std::string &filename) { return false; }
};