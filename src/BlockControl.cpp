#include "BlockControl.h"
#include "Processor.h"
using namespace std;

// TODO! make dropped no ref, remove from symbol table
// TODO! test (pos and neg) referring to other vars within drop
// TODO! what if sym or block within drop
// TODO! what if this not in block, but in func/macro, so next outer scope is global
// TODO! propagate fails in drops
BlockControl::~BlockControl() {
    if (symTable != nullptr) {
        symTable->endBlock();
    }
}