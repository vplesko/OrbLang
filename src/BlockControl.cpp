#include "BlockControl.h"
#include "Processor.h"
using namespace std;

BlockControl::~BlockControl() {
    if (symTable != nullptr) symTable->endBlock();
}