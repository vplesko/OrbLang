#pragma once

#include "LifetimeInfo.h"
#include "SymbolTableIds.h"
#include "TypeTable.h"

struct LlvmVal {
    TypeTable::Id type;
    llvm::Value *val = nullptr;
    llvm::Value *ref = nullptr;
    std::optional<VarId> varId;
    LifetimeInfo lifetimeInfo;

    LlvmVal() {}
    LlvmVal(TypeTable::Id ty) : type(ty) {}

    bool valBroken() const { return val == nullptr; }
    bool refBroken() const { return ref == nullptr; }

    void removeRef() { ref = nullptr; varId.reset(); }
};