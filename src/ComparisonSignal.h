#pragma once

#include "llvm/IR/Instructions.h"

struct ComparisonSignal {
    bool result = true;
    llvm::BasicBlock *llvmBlock = nullptr;
    llvm::PHINode *llvmPhi = nullptr;
};