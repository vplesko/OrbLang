#include "Evaluator.h"
using namespace std;

Evaluator::Evaluator(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompileMessages *msgs)
    : Processor(namePool, stringPool, typeTable, symbolTable, msgs, this) {
    resetSkipIssued();
}

bool Evaluator::isRepeatingProcessing(optional<NamePool::Id> block) const {
    return loopIssued && isSkipIssuedForCurrBlock(block);
}

NodeVal Evaluator::performLoad(CodeLoc codeLoc, NamePool::Id id, NodeVal &ref) {
    if (!checkIsKnownVal(codeLoc, ref, true)) return NodeVal();

    KnownVal knownVal(ref.getKnownVal());
    knownVal.ref = &ref.getKnownVal();
    return NodeVal(codeLoc, knownVal);
}

NodeVal Evaluator::performRegister(CodeLoc codeLoc, NamePool::Id id, TypeTable::Id ty) {
    return NodeVal(codeLoc, KnownVal::makeVal(ty, typeTable));
}

NodeVal Evaluator::performRegister(CodeLoc codeLoc, NamePool::Id id, const NodeVal &init) {
    if (!checkIsKnownVal(init, true)) return NodeVal();

    return NodeVal(codeLoc, KnownVal::copyNoRef(init.getKnownVal()));
}

NodeVal Evaluator::performCast(CodeLoc codeLoc, const NodeVal &node, TypeTable::Id ty) {
    if (!checkIsKnownVal(node, true) || !checkHasType(node, true)) return NodeVal();

    optional<KnownVal> knownValCast = makeCast(node.getKnownVal(), node.getKnownVal().type.value(), ty);
    if (!knownValCast.has_value()) {
        if (KnownVal::isImplicitCastable(node.getKnownVal(), ty, stringPool, typeTable)) {
            // if it's implicitly castable, it should be castable
            msgs->errorInternal(codeLoc);
        } else {
            msgs->errorExprCannotCast(codeLoc, node.getKnownVal().type.value(), ty);
        }
        return NodeVal();
    }

    return NodeVal(codeLoc, knownValCast.value());
}

bool Evaluator::performBlockReentry(CodeLoc codeLoc) {
    resetSkipIssued();
    return true;
}

NodeVal Evaluator::performBlockTearDown(CodeLoc codeLoc, const SymbolTable::Block &block, bool success) {
    if (!success) return NodeVal();

    // TODO figure out control flow handling and error detection
    // (eg if it evals well with a pass in the middle, but no pass at end, no error detected)
    if (!isSkipIssued() && block.type.has_value()) {
        msgs->errorBlockNoPass(codeLoc);
        return NodeVal();
    }
    
    if (isSkipIssuedForCurrBlock(block.name)) {
        resetSkipIssued();

        if (block.type.has_value()) {
            if (!block.val.has_value()) {
                msgs->errorBlockNoPass(codeLoc);
                return NodeVal();
            }

            return block.val.value();
        }
    }
    
    return NodeVal(codeLoc);
}

bool Evaluator::performExit(CodeLoc codeLoc, const SymbolTable::Block &block, const NodeVal &cond) {
    if (!checkIsKnownVal(cond, true)) return false;

    if (cond.getKnownVal().b) {
        exitOrPassIssued = true;
        // if name not given, skip until innermost block (instruction can't do otherwise)
        // if name given, skip until that block (which may even be innermost block)
        skipBlock = block.name;
    }

    return true;
}

bool Evaluator::performLoop(CodeLoc codeLoc, const SymbolTable::Block &block, const NodeVal &cond) {
    if (!checkIsKnownVal(cond, true)) return false;

    if (cond.getKnownVal().b) {
        loopIssued = true;
        // if name not given, skip until innermost block (instruction can't do otherwise)
        // if name given, skip until that block (which may even be innermost block)
        skipBlock = block.name;
    }

    return true;
}

bool Evaluator::performPass(CodeLoc codeLoc, SymbolTable::Block &block, const NodeVal &val) {
    if (!checkIsKnownVal(val, true)) return false;

    exitOrPassIssued = true;
    skipBlock = block.name;
    block.val = NodeVal(codeLoc, KnownVal::copyNoRef(val.getKnownVal()));
    
    return true;
}

NodeVal Evaluator::performCall(CodeLoc codeLoc, const FuncValue &func, const std::vector<NodeVal> &args) {
    if (!func.knownFunc.has_value()) {
        msgs->errorUnknown(codeLoc);
        return NodeVal();
    }

    if (func.knownFunc.value().isInvalid()) {
        msgs->errorUnknown(codeLoc);
        return NodeVal();
    }

    BlockControl blockCtrl(*symbolTable, func);

    for (size_t i = 0; i < args.size(); ++i) {
        symbolTable->addVar(func.argNames[i], args[i]);
    }

    if (!processChildNodes(func.knownFunc.value())) {
        return NodeVal();
    }

    if (func.hasRet()) {
        if (!retVal.has_value()) {
            msgs->errorRetNoValue(codeLoc, func.retType.value());
            return NodeVal();
        }

        NodeVal ret = move(retVal.value());
        resetSkipIssued();
        return move(ret);
    } else {
        resetSkipIssued();
        return NodeVal(codeLoc);
    }
}

bool Evaluator::performFunctionDeclaration(CodeLoc codeLoc, FuncValue &func) {
    // mark the func as eval, but it cannot be called
    func.knownFunc = NodeVal();
    return true;
}

bool Evaluator::performFunctionDefinition(const NodeVal &args, const NodeVal &body, FuncValue &func) {
    func.knownFunc = body;
    return true;
}

bool Evaluator::performRet(CodeLoc codeLoc) {
    retIssued = true;
    return true;
}

bool Evaluator::performRet(CodeLoc codeLoc, const NodeVal &node) {
    retIssued = true;
    retVal = node;
    return true;
}

NodeVal Evaluator::performOperUnary(CodeLoc codeLoc, const NodeVal &oper, Oper op) {
    if (!checkIsKnownVal(oper, true) || !checkHasType(oper, true)) return NodeVal();

    KnownVal knownVal = KnownVal::copyNoRef(oper.getKnownVal());
    TypeTable::Id ty = knownVal.type.value();
    bool success = false, errorGiven = false;
    if (op == Oper::ADD) {
        if (KnownVal::isI(knownVal, typeTable) ||
            KnownVal::isU(knownVal, typeTable) ||
            KnownVal::isF(knownVal, typeTable)) {
            // do nothing to operand
            success = true;
        }
    } else if (op == Oper::SUB) {
        if (KnownVal::isI(knownVal, typeTable)) {
            int64_t x = KnownVal::getValueI(knownVal, typeTable).value();
            x = -x;
            if (assignBasedOnTypeI(knownVal, (int64_t) x, ty)) success = true;
        } else if (KnownVal::isF(knownVal, typeTable)) {
            double x = KnownVal::getValueF(knownVal, typeTable).value();
            x = -x;
            if (assignBasedOnTypeF(knownVal, x, ty)) success = true;
        }
    } else if (op == Oper::BIT_NOT) {
        if (KnownVal::isI(knownVal, typeTable)) {
            int64_t x = KnownVal::getValueI(knownVal, typeTable).value();
            x = ~x;
            if (assignBasedOnTypeI(knownVal, (int64_t) x, ty)) success = true;
        } else if (KnownVal::isU(knownVal, typeTable)) {
            uint64_t x = KnownVal::getValueU(knownVal, typeTable).value();
            x = ~x;
            if (assignBasedOnTypeU(knownVal, (uint64_t) x, ty)) success = true;
        }
    } else if (op == Oper::NOT) {
        if (KnownVal::isB(knownVal, typeTable)) {
            knownVal.b = !knownVal.b;
            success = true;
        }
    } else if (op == Oper::BIT_AND) {
        msgs->errorEvaluationNotSupported(codeLoc);
        errorGiven = true;
    }

    if (!success) {
        if (!errorGiven) msgs->errorExprUnBadType(codeLoc);
        return NodeVal();
    }

    return NodeVal(codeLoc, knownVal);
}

NodeVal Evaluator::performOperUnaryDeref(CodeLoc codeLoc, const NodeVal &oper) {
    msgs->errorEvaluationNotSupported(codeLoc);
    return NodeVal();
}

void* Evaluator::performOperComparisonSetUp(CodeLoc codeLoc, std::size_t opersCnt) {
    return new ComparisonSignal;
}

optional<bool> Evaluator::performOperComparison(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op, void *signal) {
    ComparisonSignal *result = (ComparisonSignal*) signal;

    if (!checkIsKnownVal(lhs, true) || !checkIsKnownVal(rhs, true)) return nullopt;

    TypeTable::Id ty = lhs.getType().value();
    
    bool isTypeI = typeTable->worksAsTypeI(ty);
    bool isTypeU = typeTable->worksAsTypeU(ty);
    bool isTypeC = typeTable->worksAsTypeC(ty);
    bool isTypeF = typeTable->worksAsTypeF(ty);
    bool isTypeStr = typeTable->worksAsTypeStr(ty);
    bool isTypeP = typeTable->worksAsTypeAnyP(ty);
    bool isTypeB = typeTable->worksAsTypeB(ty);

    optional<int64_t> il, ir;
    optional<uint64_t> ul, ur;
    optional<char> cl, cr;
    optional<double> fl, fr;
    optional<StringPool::Id> strl, strr;
    optional<bool> bl, br;
    if (isTypeI) {
        il = KnownVal::getValueI(lhs.getKnownVal(), typeTable).value();
        ir = KnownVal::getValueI(rhs.getKnownVal(), typeTable).value();
    } else if (isTypeU) {
        ul = KnownVal::getValueU(lhs.getKnownVal(), typeTable).value();
        ur = KnownVal::getValueU(rhs.getKnownVal(), typeTable).value();
    } else if (isTypeC) {
        cl = lhs.getKnownVal().c8;
        cr = rhs.getKnownVal().c8;
    } else if (isTypeF) {
        fl = KnownVal::getValueF(lhs.getKnownVal(), typeTable).value();
        fr = KnownVal::getValueF(rhs.getKnownVal(), typeTable).value();
    } else if (isTypeStr) {
        strl = lhs.getKnownVal().str.value();
        strr = rhs.getKnownVal().str.value();
    } else if (isTypeB) {
        bl = lhs.getKnownVal().b;
        br = rhs.getKnownVal().b;
    }

    switch (op) {
    case Oper::EQ:
        if (isTypeI) {
            result->result = il.value()==ir.value();
            return !result->result;
        } else if (isTypeU) {
            result->result = ul.value()==ur.value();
            return !result->result;
        } else if (isTypeC) {
            result->result = cl.value()==cr.value();
            return !result->result;
        } else if (isTypeF) {
            result->result = fl.value()==fr.value();
            return !result->result;
        } else if (isTypeStr) {
            result->result = strl.value()==strr.value();
            return !result->result;
        } else if (isTypeP) {
            result->result = true; // null == null
            return !result->result;
        } else if (isTypeB) {
            result->result = bl.value()==br.value();
            return !result->result;
        }
        break;
    case Oper::NE:
        if (isTypeI) {
            result->result = il.value()!=ir.value();
            return !result->result;
        } else if (isTypeU) {
            result->result = ul.value()!=ur.value();
            return !result->result;
        } else if (isTypeC) {
            result->result = cl.value()!=cr.value();
            return !result->result;
        } else if (isTypeF) {
            result->result = fl.value()!=fr.value();
            return !result->result;
        } else if (isTypeStr) {
            result->result = strl.value()!=strr.value();
            return !result->result;
        } else if (isTypeP) {
            result->result = false; // null != null
            return !result->result;
        } else if (isTypeB) {
            result->result = bl.value()!=br.value();
            return !result->result;
        }
        break;
    case Oper::LT:
        if (isTypeI) {
            result->result = il.value()<ir.value();
            return !result->result;
        } else if (isTypeU) {
            result->result = ul.value()<ur.value();
            return !result->result;
        } else if (isTypeC) {
            result->result = cl.value()<cr.value();
            return !result->result;
        } else if (isTypeF) {
            result->result = fl.value()<fr.value();
            return !result->result;
        }
        break;
    case Oper::LE:
        if (isTypeI) {
            result->result = il.value()<=ir.value();
            return !result->result;
        } else if (isTypeU) {
            result->result = ul.value()<=ur.value();
            return !result->result;
        } else if (isTypeC) {
            result->result = cl.value()<=cr.value();
            return !result->result;
        } else if (isTypeF) {
            result->result = fl.value()<=fr.value();
            return !result->result;
        }
        break;
    case Oper::GT:
        if (isTypeI) {
            result->result = il.value()>ir.value();
            return !result->result;
        } else if (isTypeU) {
            result->result = ul.value()>ur.value();
            return !result->result;
        } else if (isTypeC) {
            result->result = cl.value()>cr.value();
            return !result->result;
        } else if (isTypeF) {
            result->result = fl.value()>fr.value();
            return !result->result;
        }
        break;
    case Oper::GE:
        if (isTypeI) {
            result->result = il.value()>=ir.value();
            return !result->result;
        } else if (isTypeU) {
            result->result = ul.value()>=ur.value();
            return !result->result;
        } else if (isTypeC) {
            result->result = cl.value()>=cr.value();
            return !result->result;
        } else if (isTypeF) {
            result->result = fl.value()>=fr.value();
            return !result->result;
        }
        break;
    }

    msgs->errorExprKnownBinBadOp(codeLoc);
    return nullopt;    
}

NodeVal Evaluator::performOperComparisonTearDown(CodeLoc codeLoc, bool success, void *signal) {
    if (!success) return NodeVal();

    KnownVal knownVal = KnownVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_BOOL), typeTable);
    knownVal.b = ((ComparisonSignal*) signal)->result;
    return NodeVal(codeLoc, knownVal);
}

NodeVal Evaluator::performOperAssignment(CodeLoc codeLoc, NodeVal &lhs, const NodeVal &rhs) {
    if (!checkIsKnownVal(lhs, true) || !checkIsKnownVal(rhs, true)) return NodeVal();

    *lhs.getKnownVal().ref = KnownVal::copyNoRef(rhs.getKnownVal());

    KnownVal knownVal(rhs.getKnownVal());
    knownVal.ref = lhs.getKnownVal().ref;
    return NodeVal(codeLoc, knownVal);
}

NodeVal Evaluator::performOperIndex(CodeLoc codeLoc, NodeVal &base, const NodeVal &ind, TypeTable::Id resTy) {
    if (!checkIsKnownVal(base, true) || !checkIsKnownVal(ind, true)) return NodeVal();

    optional<size_t> index = KnownVal::getValueNonNeg(ind.getKnownVal(), typeTable);
    if (!index.has_value()) {
        msgs->errorInternal(codeLoc);
        return NodeVal();
    }

    KnownVal knownVal;
    if (typeTable->worksAsTypeArr(base.getType().value())) {
        knownVal = KnownVal(base.getKnownVal().elems[index.value()]);
        if (base.hasRef()) {
            knownVal.ref = &base.getKnownVal().ref->elems[index.value()];
        } else {
            knownVal.ref = nullptr;
        }
    } else if (typeTable->worksAsTypeArrP(base.getType().value())) {
        msgs->errorUnknown(codeLoc);
        return NodeVal();
    } else {
        msgs->errorInternal(codeLoc);
        return NodeVal();
    }
    return NodeVal(codeLoc, knownVal);
}

NodeVal Evaluator::performOperMember(CodeLoc codeLoc, NodeVal &base, std::uint64_t ind, TypeTable::Id resTy) {
    if (!checkIsKnownVal(base, true)) return NodeVal();

    KnownVal knownVal(base.getKnownVal().elems[ind]);
    if (base.hasRef()) {
        knownVal.ref = &base.getKnownVal().ref->elems[ind];
    } else {
        knownVal.ref = nullptr;
    }
    return NodeVal(codeLoc, knownVal);
}

// TODO warn on over/underflow; are results correct in these cases?
NodeVal Evaluator::performOperRegular(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op) {
    if (!checkIsKnownVal(lhs, true) || !checkIsKnownVal(rhs, true)) return NodeVal();
    
    TypeTable::Id ty = lhs.getType().value();
    KnownVal knownVal = KnownVal::makeVal(ty, typeTable);
    bool success = false, errorGiven = false;

    bool isTypeI = typeTable->worksAsTypeI(ty);
    bool isTypeU = typeTable->worksAsTypeU(ty);
    bool isTypeF = typeTable->worksAsTypeF(ty);

    optional<int64_t> il, ir;
    optional<uint64_t> ul, ur;
    optional<double> fl, fr;
    if (isTypeI) {
        il = KnownVal::getValueI(lhs.getKnownVal(), typeTable).value();
        ir = KnownVal::getValueI(rhs.getKnownVal(), typeTable).value();
    } else if (isTypeU) {
        ul = KnownVal::getValueU(lhs.getKnownVal(), typeTable).value();
        ur = KnownVal::getValueU(rhs.getKnownVal(), typeTable).value();
    } else if (isTypeF) {
        fl = KnownVal::getValueF(lhs.getKnownVal(), typeTable).value();
        fr = KnownVal::getValueF(rhs.getKnownVal(), typeTable).value();
    }
    
    switch (op) {
    case Oper::ADD:
        if (isTypeI) {
            if (assignBasedOnTypeI(knownVal, il.value()+ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(knownVal, ul.value()+ur.value(), ty)) success = true;
        } else if (isTypeF) {
            if (assignBasedOnTypeF(knownVal, fl.value()+fr.value(), ty)) success = true;
        }
        break;
    case Oper::SUB:
        if (isTypeI) {
            if (assignBasedOnTypeI(knownVal, il.value()-ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(knownVal, ul.value()-ur.value(), ty)) success = true;
        } else if (isTypeF) {
            if (assignBasedOnTypeF(knownVal, fl.value()-fr.value(), ty)) success = true;
        }
        break;
    case Oper::MUL:
        if (isTypeI) {
            if (assignBasedOnTypeI(knownVal, il.value()*ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(knownVal, ul.value()*ur.value(), ty)) success = true;
        } else if (isTypeF) {
            if (assignBasedOnTypeF(knownVal, fl.value()*fr.value(), ty)) success = true;
        }
        break;
    case Oper::DIV:
        if (isTypeI) {
            if (assignBasedOnTypeI(knownVal, il.value()/ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(knownVal, ul.value()/ur.value(), ty)) success = true;
        } else if (isTypeF) {
            if (assignBasedOnTypeF(knownVal, fl.value()/fr.value(), ty)) success = true;
        }
        break;
    case Oper::REM:
        if (isTypeI) {
            if (assignBasedOnTypeI(knownVal, il.value()%ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(knownVal, ul.value()%ur.value(), ty)) success = true;
        } else if (isTypeF) {
            if (typeTable->worksAsPrimitive(ty, TypeTable::P_F32)) {
                knownVal.f32 = fmod(lhs.getKnownVal().f32, rhs.getKnownVal().f32);
            } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_F64)) {
                knownVal.f64 = fmod(lhs.getKnownVal().f64, rhs.getKnownVal().f64);
            }
            success = true;
        }
        break;
    case Oper::SHL:
        if (isTypeI) {
            if (assignBasedOnTypeI(knownVal, il.value()<<ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(knownVal, ul.value()<<ur.value(), ty)) success = true;
        }
        break;
    case Oper::SHR:
        if (isTypeI) {
            if (assignBasedOnTypeI(knownVal, il.value()>>ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(knownVal, ul.value()>>ur.value(), ty)) success = true;
        }
        break;
    case Oper::BIT_AND:
        if (isTypeI) {
            if (assignBasedOnTypeI(knownVal, il.value()&ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(knownVal, ul.value()&ur.value(), ty)) success = true;
        }
        break;
    case Oper::BIT_OR:
        if (isTypeI) {
            if (assignBasedOnTypeI(knownVal, il.value()|ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(knownVal, ul.value()|ur.value(), ty)) success = true;
        }
        break;
    case Oper::BIT_XOR:
        if (isTypeI) {
            if (assignBasedOnTypeI(knownVal, il.value()^ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(knownVal, ul.value()^ur.value(), ty)) success = true;
        }
        break;
    }

    if (!success) {
        if (!errorGiven) msgs->errorExprKnownBinBadOp(codeLoc);
        return NodeVal();
    }

    return NodeVal(codeLoc, knownVal);
}

NodeVal Evaluator::performTuple(CodeLoc codeLoc, TypeTable::Id ty, const std::vector<NodeVal> &membs) {
    KnownVal knownVal = KnownVal::makeVal(ty, typeTable);

    for (size_t i = 0; i < membs.size(); ++i) {
        const NodeVal &memb = membs[i];
        if (!checkIsKnownVal(memb, true)) return NodeVal();
        
        knownVal.elems[i] = KnownVal::copyNoRef(memb.getKnownVal());
    }

    return NodeVal(codeLoc, knownVal);
}

// TODO warn on lossy
optional<KnownVal> Evaluator::makeCast(const KnownVal &srcKnownVal, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId) {
    if (srcTypeId == dstTypeId) return KnownVal::copyNoRef(srcKnownVal);

    optional<KnownVal> dstKnownVal = KnownVal::makeVal(dstTypeId, typeTable);

    if (typeTable->worksAsTypeI(srcTypeId)) {
        int64_t x = KnownVal::getValueI(srcKnownVal, typeTable).value();
        if (!assignBasedOnTypeI(dstKnownVal.value(), (int64_t) x, dstTypeId) &&
            !assignBasedOnTypeU(dstKnownVal.value(), (uint64_t) x, dstTypeId) &&
            !assignBasedOnTypeF(dstKnownVal.value(), (double) x, dstTypeId) &&
            !assignBasedOnTypeC(dstKnownVal.value(), (char) x, dstTypeId) &&
            !assignBasedOnTypeB(dstKnownVal.value(), (bool) x, dstTypeId) &&
            !(typeTable->worksAsTypeStr(dstTypeId) && x == 0) &&
            !(typeTable->worksAsTypeAnyP(dstTypeId) && x == 0)) {
            dstKnownVal.reset();
        }
    } else if (typeTable->worksAsTypeU(srcTypeId)) {
        uint64_t x = KnownVal::getValueU(srcKnownVal, typeTable).value();
        if (!assignBasedOnTypeI(dstKnownVal.value(), (int64_t) x, dstTypeId) &&
            !assignBasedOnTypeU(dstKnownVal.value(), (uint64_t) x, dstTypeId) &&
            !assignBasedOnTypeF(dstKnownVal.value(), (double) x, dstTypeId) &&
            !assignBasedOnTypeC(dstKnownVal.value(), (char) x, dstTypeId) &&
            !assignBasedOnTypeB(dstKnownVal.value(), (bool) x, dstTypeId) &&
            !(typeTable->worksAsTypeStr(dstTypeId) && x == 0) &&
            !(typeTable->worksAsTypeAnyP(dstTypeId) && x == 0)) {
            dstKnownVal.reset();
        }
    } else if (typeTable->worksAsTypeF(srcTypeId)) {
        double x = KnownVal::getValueF(srcKnownVal, typeTable).value();
        if (!assignBasedOnTypeI(dstKnownVal.value(), (int64_t) x, dstTypeId) &&
            !assignBasedOnTypeU(dstKnownVal.value(), (uint64_t) x, dstTypeId) &&
            !assignBasedOnTypeF(dstKnownVal.value(), (double) x, dstTypeId)) {
            dstKnownVal.reset();
        }
    } else if (typeTable->worksAsTypeC(srcTypeId)) {
        if (!assignBasedOnTypeI(dstKnownVal.value(), (int64_t) srcKnownVal.c8, dstTypeId) &&
            !assignBasedOnTypeU(dstKnownVal.value(), (uint64_t) srcKnownVal.c8, dstTypeId) &&
            !assignBasedOnTypeC(dstKnownVal.value(), srcKnownVal.c8, dstTypeId) &&
            !assignBasedOnTypeB(dstKnownVal.value(), (bool) srcKnownVal.c8, dstTypeId)) {
            dstKnownVal.reset();
        }
    } else if (typeTable->worksAsTypeB(srcTypeId)) {
        if (!assignBasedOnTypeI(dstKnownVal.value(), srcKnownVal.b ? 1 : 0, dstTypeId) &&
            !assignBasedOnTypeU(dstKnownVal.value(), srcKnownVal.b ? 1 : 0, dstTypeId) &&
            !assignBasedOnTypeB(dstKnownVal.value(), srcKnownVal.b, dstTypeId)) {
            dstKnownVal.reset();
        }
    } else if (typeTable->worksAsTypeStr(srcTypeId)) {
        if (srcKnownVal.str.has_value()) {
            const string &str = stringPool->get(srcKnownVal.str.value());
            if (typeTable->worksAsTypeStr(dstTypeId)) {
                dstKnownVal.value().str = srcKnownVal.str;
            } else if (typeTable->worksAsTypeB(dstTypeId)) {
                dstKnownVal.value().b = true;
            } else if (typeTable->worksAsTypeCharArrOfLen(dstTypeId, LiteralVal::getStringLen(str))) {
                dstKnownVal = makeArray(dstTypeId);
                if (!dstKnownVal.has_value()) { 
                    dstKnownVal.reset();
                } else {
                    for (size_t i = 0; i < LiteralVal::getStringLen(str); ++i) {
                        dstKnownVal.value().elems[i].c8 = str[i];
                    }
                }
            } else {
                dstKnownVal.reset();
            }
        } else {
            // if it's not string, it's null
            if (!assignBasedOnTypeI(dstKnownVal.value(), 0, dstTypeId) &&
                !assignBasedOnTypeU(dstKnownVal.value(), 0, dstTypeId) &&
                !assignBasedOnTypeB(dstKnownVal.value(), false, dstTypeId) &&
                !typeTable->worksAsTypeAnyP(dstTypeId)) {
                dstKnownVal.reset();
            }
        }
    } else if (typeTable->worksAsTypeAnyP(srcTypeId)) {
        // it's null
        if (!assignBasedOnTypeI(dstKnownVal.value(), 0, dstTypeId) &&
            !assignBasedOnTypeU(dstKnownVal.value(), 0, dstTypeId) &&
            !assignBasedOnTypeB(dstKnownVal.value(), false, dstTypeId) &&
            !typeTable->worksAsTypeAnyP(dstTypeId)) {
            dstKnownVal.reset();
        }
    } else if (typeTable->worksAsTypeArr(srcTypeId) || typeTable->worksAsTuple(srcTypeId) ||
        typeTable->worksAsPrimitive(srcTypeId, TypeTable::P_ID) || typeTable->worksAsPrimitive(srcTypeId, TypeTable::P_TYPE)) {
        // these types are only castable when changing constness
        if (typeTable->isImplicitCastable(srcTypeId, dstTypeId)) {
            // no action is needed in case of a cast
            dstKnownVal = srcKnownVal;
            dstKnownVal.value().type = dstTypeId;
            dstKnownVal.value().ref = nullptr;
        } else {
            dstKnownVal.reset();
        }
    } else {
        dstKnownVal.reset();
    }

    return dstKnownVal;
}

optional<KnownVal> Evaluator::makeArray(TypeTable::Id arrTypeId) {
    if (!typeTable->worksAsTypeArr(arrTypeId)) return nullopt;

    return KnownVal::makeVal(arrTypeId, typeTable);
}

bool Evaluator::assignBasedOnTypeI(KnownVal &val, int64_t x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_I8)) {
        val.i8 = (int8_t) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_I16)) {
        val.i16 = (int16_t) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_I32)) {
        val.i32 = (int32_t) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_I64)) {
        val.i64 = (int64_t) x;
    } else {
        return false;
    }

    return true;
}

bool Evaluator::assignBasedOnTypeU(KnownVal &val, uint64_t x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_U8)) {
        val.u8 = (uint8_t) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_U16)) {
        val.u16 = (uint16_t) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_U32)) {
        val.u32 = (uint32_t) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_U64)) {
        val.u64 = (uint64_t) x;
    } else {
        return false;
    }

    return true;
}

bool Evaluator::assignBasedOnTypeF(KnownVal &val, double x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_F32)) {
        val.f32 = (float) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_F64)) {
        val.f64 = (double) x;
    } else {
        return false;
    }

    return true;
}

bool Evaluator::assignBasedOnTypeC(KnownVal &val, char x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_C8)) {
        val.c8 = x;
    } else {
        return false;
    }

    return true;
}

bool Evaluator::assignBasedOnTypeB(KnownVal &val, bool x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_BOOL)) {
        val.b = x;
    } else {
        return false;
    }

    return true;
}

bool Evaluator::isSkipIssued() const {
    return exitOrPassIssued || loopIssued || retIssued;
}

bool Evaluator::isSkipIssuedNotRet() const {
    return exitOrPassIssued || loopIssued;
}

bool Evaluator::isSkipIssuedForCurrBlock(optional<NamePool::Id> currBlockName) const {
    return isSkipIssuedNotRet() && (!skipBlock.has_value() || skipBlock == currBlockName);
}

void Evaluator::resetSkipIssued() {
    exitOrPassIssued = false;
    loopIssued = false;
    skipBlock.reset();
    retIssued = false;
    retVal.reset();
}