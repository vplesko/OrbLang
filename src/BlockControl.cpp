#include "BlockControl.h"
#include "Processor.h"
using namespace std;

// TODO! test (pos and neg) referring to other vars within drop
// TODO! test block within drop
BlockControl::~BlockControl() {
    if (symbolTable != nullptr) {
        if (processor != nullptr) {
            SymbolTable::BlockInternal *lastBlock = symbolTable->getLastBlockInternal();
            lastBlock->dropsOngoing = true;

            vector<SymbolTable::VarEntry*> vars = lastBlock->varsInOrder;
            for (auto it = vars.rbegin(); it != vars.rend(); ++it) {
                NodeVal var = move((*it)->var);

                (*it)->dropped = true;
                var.removeRef();

                // if fails, will be detected after func/macro is done processing
                processor->invokeDrop(move(var));
            }
        }
    
        symbolTable->endBlock();
    }
}