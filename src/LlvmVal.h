#pragma once

#include "TypeTable.h"

struct LlvmVal {
    TypeTable::Id type;
    llvm::Value *val = nullptr;
    llvm::Value *ref = nullptr;

    LlvmVal() {}
    LlvmVal(TypeTable::Id ty) : type(ty) {}

    bool valBroken() const { return val == nullptr; }
    bool refBroken() const { return ref == nullptr; }
};