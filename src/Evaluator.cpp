#include "Evaluator.h"
using namespace std;

struct ComparisonSignal {
    bool result = true;
};

Evaluator::Evaluator(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompilationMessages *msgs)
    : Processor(namePool, stringPool, typeTable, symbolTable, msgs, this) {
    resetSkipIssued();
}

bool Evaluator::isRepeatingProcessing(optional<NamePool::Id> block) const {
    return loopIssued && isSkipIssuedForCurrBlock(block);
}

NodeVal Evaluator::performLoad(CodeLoc codeLoc, NamePool::Id id, NodeVal &ref) {
    if (!checkIsEvalVal(codeLoc, ref, true)) return NodeVal();

    EvalVal evalVal(ref.getEvalVal());
    evalVal.ref = &ref;
    return NodeVal(codeLoc, evalVal);
}

NodeVal Evaluator::performRegister(CodeLoc codeLoc, NamePool::Id id, TypeTable::Id ty) {
    return NodeVal(codeLoc, EvalVal::makeVal(ty, typeTable));
}

NodeVal Evaluator::performRegister(CodeLoc codeLoc, NamePool::Id id, const NodeVal &init) {
    if (!checkIsEvalVal(init, true)) return NodeVal();

    return NodeVal(codeLoc, EvalVal::copyNoRef(init.getEvalVal()));
}

NodeVal Evaluator::performCast(CodeLoc codeLoc, const NodeVal &node, TypeTable::Id ty) {
    if (!checkIsEvalVal(node, true) || !checkHasType(node, true)) return NodeVal();

    optional<EvalVal> evalValCast = makeCast(node.getEvalVal(), node.getEvalVal().type.value(), ty);
    if (!evalValCast.has_value()) {
        if (EvalVal::isImplicitCastable(node.getEvalVal(), ty, stringPool, typeTable)) {
            // if it's implicitly castable, it should be castable
            msgs->errorInternal(codeLoc);
        } else {
            msgs->errorExprCannotCast(codeLoc, node.getEvalVal().type.value(), ty);
        }
        return NodeVal();
    }

    return NodeVal(codeLoc, evalValCast.value());
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
    
    return NodeVal(true);
}

bool Evaluator::performExit(CodeLoc codeLoc, const SymbolTable::Block &block, const NodeVal &cond) {
    if (!checkIsEvalVal(cond, true)) return false;

    if (cond.getEvalVal().b) {
        exitOrPassIssued = true;
        // if name not given, skip until innermost block (instruction can't do otherwise)
        // if name given, skip until that block (which may even be innermost block)
        skipBlock = block.name;
    }

    return true;
}

bool Evaluator::performLoop(CodeLoc codeLoc, const SymbolTable::Block &block, const NodeVal &cond) {
    if (!checkIsEvalVal(cond, true)) return false;

    if (cond.getEvalVal().b) {
        loopIssued = true;
        // if name not given, skip until innermost block (instruction can't do otherwise)
        // if name given, skip until that block (which may even be innermost block)
        skipBlock = block.name;
    }

    return true;
}

bool Evaluator::performPass(CodeLoc codeLoc, SymbolTable::Block &block, const NodeVal &val) {
    if (!checkIsEvalVal(val, true)) return false;

    exitOrPassIssued = true;
    skipBlock = block.name;
    block.val = NodeVal(codeLoc, EvalVal::copyNoRef(val.getEvalVal()));
    
    return true;
}

NodeVal Evaluator::performCall(CodeLoc codeLoc, const FuncValue &func, const std::vector<NodeVal> &args) {
    if (!func.evalFunc.has_value()) {
        msgs->errorUnknown(codeLoc);
        return NodeVal();
    }

    if (func.evalFunc.value().isInvalid()) {
        msgs->errorUnknown(codeLoc);
        return NodeVal();
    }

    BlockControl blockCtrl(symbolTable, func);

    for (size_t i = 0; i < args.size(); ++i) {
        symbolTable->addVar(func.argNames[i], args[i]);
    }

    if (!processChildNodes(func.evalFunc.value())) {
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
        return NodeVal(true);
    }
}

bool Evaluator::performFunctionDeclaration(CodeLoc codeLoc, FuncValue &func) {
    // mark the func as eval, but it cannot be called
    func.evalFunc = NodeVal();
    return true;
}

bool Evaluator::performFunctionDefinition(const NodeVal &args, const NodeVal &body, FuncValue &func) {
    func.evalFunc = body;
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
    if (!checkIsEvalVal(oper, true) || !checkHasType(oper, true)) return NodeVal();

    EvalVal evalVal = EvalVal::copyNoRef(oper.getEvalVal());
    TypeTable::Id ty = evalVal.type.value();
    bool success = false, errorGiven = false;
    if (op == Oper::ADD) {
        if (EvalVal::isI(evalVal, typeTable) ||
            EvalVal::isU(evalVal, typeTable) ||
            EvalVal::isF(evalVal, typeTable)) {
            // do nothing to operand
            success = true;
        }
    } else if (op == Oper::SUB) {
        if (EvalVal::isI(evalVal, typeTable)) {
            int64_t x = EvalVal::getValueI(evalVal, typeTable).value();
            x = -x;
            if (assignBasedOnTypeI(evalVal, (int64_t) x, ty)) success = true;
        } else if (EvalVal::isF(evalVal, typeTable)) {
            double x = EvalVal::getValueF(evalVal, typeTable).value();
            x = -x;
            if (assignBasedOnTypeF(evalVal, x, ty)) success = true;
        }
    } else if (op == Oper::BIT_NOT) {
        if (EvalVal::isI(evalVal, typeTable)) {
            int64_t x = EvalVal::getValueI(evalVal, typeTable).value();
            x = ~x;
            if (assignBasedOnTypeI(evalVal, (int64_t) x, ty)) success = true;
        } else if (EvalVal::isU(evalVal, typeTable)) {
            uint64_t x = EvalVal::getValueU(evalVal, typeTable).value();
            x = ~x;
            if (assignBasedOnTypeU(evalVal, (uint64_t) x, ty)) success = true;
        }
    } else if (op == Oper::NOT) {
        if (EvalVal::isB(evalVal, typeTable)) {
            evalVal.b = !evalVal.b;
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

    return NodeVal(codeLoc, evalVal);
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

    if (!checkIsEvalVal(lhs, true) || !checkIsEvalVal(rhs, true)) return nullopt;

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
        il = EvalVal::getValueI(lhs.getEvalVal(), typeTable).value();
        ir = EvalVal::getValueI(rhs.getEvalVal(), typeTable).value();
    } else if (isTypeU) {
        ul = EvalVal::getValueU(lhs.getEvalVal(), typeTable).value();
        ur = EvalVal::getValueU(rhs.getEvalVal(), typeTable).value();
    } else if (isTypeC) {
        cl = lhs.getEvalVal().c8;
        cr = rhs.getEvalVal().c8;
    } else if (isTypeF) {
        fl = EvalVal::getValueF(lhs.getEvalVal(), typeTable).value();
        fr = EvalVal::getValueF(rhs.getEvalVal(), typeTable).value();
    } else if (isTypeStr) {
        strl = lhs.getEvalVal().str.value();
        strr = rhs.getEvalVal().str.value();
    } else if (isTypeB) {
        bl = lhs.getEvalVal().b;
        br = rhs.getEvalVal().b;
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

    msgs->errorExprEvalBinBadOp(codeLoc);
    return nullopt;    
}

NodeVal Evaluator::performOperComparisonTearDown(CodeLoc codeLoc, bool success, void *signal) {
    if (!success) return NodeVal();

    EvalVal evalVal = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_BOOL), typeTable);
    evalVal.b = ((ComparisonSignal*) signal)->result;
    return NodeVal(codeLoc, evalVal);
}

NodeVal Evaluator::performOperAssignment(CodeLoc codeLoc, NodeVal &lhs, const NodeVal &rhs) {
    if (!checkIsEvalVal(lhs, true) || !checkIsEvalVal(rhs, true)) return NodeVal();

    *lhs.getEvalVal().ref = NodeVal(lhs.getEvalVal().ref->getCodeLoc(), EvalVal::copyNoRef(rhs.getEvalVal()));

    EvalVal evalVal(rhs.getEvalVal());
    evalVal.ref = lhs.getEvalVal().ref;
    return NodeVal(codeLoc, evalVal);
}

NodeVal Evaluator::performOperIndex(CodeLoc codeLoc, NodeVal &base, const NodeVal &ind, TypeTable::Id resTy) {
    if (!checkIsEvalVal(base, true) || !checkIsEvalVal(ind, true)) return NodeVal();

    optional<size_t> index = EvalVal::getValueNonNeg(ind.getEvalVal(), typeTable);
    if (!index.has_value()) {
        msgs->errorInternal(codeLoc);
        return NodeVal();
    }

    EvalVal evalVal;
    if (typeTable->worksAsTypeArr(base.getType().value())) {
        evalVal = EvalVal(base.getEvalVal().elems[index.value()].getEvalVal());
        if (base.hasRef()) {
            evalVal.ref = &base.getEvalVal().ref->getEvalVal().elems[index.value()];
        } else {
            evalVal.ref = nullptr;
        }
    } else if (typeTable->worksAsTypeArrP(base.getType().value())) {
        msgs->errorUnknown(codeLoc);
        return NodeVal();
    } else {
        msgs->errorInternal(codeLoc);
        return NodeVal();
    }
    return NodeVal(codeLoc, evalVal);
}

NodeVal Evaluator::performOperMember(CodeLoc codeLoc, NodeVal &base, std::uint64_t ind, TypeTable::Id resTy) {
    if (!checkIsEvalVal(base, true)) return NodeVal();

    if (NodeVal::isRawVal(base, typeTable)) {
        NodeVal nodeVal = base.getChild(ind);
        if (NodeVal::isRawVal(nodeVal, typeTable)) {
            nodeVal.getEvalVal().type = resTy;
            if (base.hasRef()) {
                nodeVal.getEvalVal().ref = &base.getEvalVal().ref->getEvalVal().elems[ind];
            } else {
                nodeVal.getEvalVal().ref = nullptr;
            }
        }
        return nodeVal;
    } else {
        EvalVal evalVal(base.getEvalVal().elems[ind].getEvalVal());
        evalVal.type = resTy;
        if (base.hasRef()) {
            evalVal.ref = &base.getEvalVal().ref->getEvalVal().elems[ind];
        } else {
            evalVal.ref = nullptr;
        }
        return NodeVal(codeLoc, evalVal);
    }
}

// TODO warn on over/underflow; are results correct in these cases?
NodeVal Evaluator::performOperRegular(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op) {
    if (!checkIsEvalVal(lhs, true) || !checkIsEvalVal(rhs, true)) return NodeVal();
    
    TypeTable::Id ty = lhs.getType().value();
    EvalVal evalVal = EvalVal::makeVal(ty, typeTable);
    bool success = false, errorGiven = false;

    bool isTypeI = typeTable->worksAsTypeI(ty);
    bool isTypeU = typeTable->worksAsTypeU(ty);
    bool isTypeF = typeTable->worksAsTypeF(ty);
    bool isTypeRaw = NodeVal::isRawVal(lhs, typeTable);

    optional<int64_t> il, ir;
    optional<uint64_t> ul, ur;
    optional<double> fl, fr;
    if (isTypeI) {
        il = EvalVal::getValueI(lhs.getEvalVal(), typeTable).value();
        ir = EvalVal::getValueI(rhs.getEvalVal(), typeTable).value();
    } else if (isTypeU) {
        ul = EvalVal::getValueU(lhs.getEvalVal(), typeTable).value();
        ur = EvalVal::getValueU(rhs.getEvalVal(), typeTable).value();
    } else if (isTypeF) {
        fl = EvalVal::getValueF(lhs.getEvalVal(), typeTable).value();
        fr = EvalVal::getValueF(rhs.getEvalVal(), typeTable).value();
    }
    
    switch (op) {
    case Oper::ADD:
        if (isTypeI) {
            if (assignBasedOnTypeI(evalVal, il.value()+ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(evalVal, ul.value()+ur.value(), ty)) success = true;
        } else if (isTypeF) {
            if (assignBasedOnTypeF(evalVal, fl.value()+fr.value(), ty)) success = true;
        } else if (isTypeRaw) {
            evalVal.elems = makeRawConcat(lhs.getEvalVal(), rhs.getEvalVal());
            success = true;
        }
        break;
    case Oper::SUB:
        if (isTypeI) {
            if (assignBasedOnTypeI(evalVal, il.value()-ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(evalVal, ul.value()-ur.value(), ty)) success = true;
        } else if (isTypeF) {
            if (assignBasedOnTypeF(evalVal, fl.value()-fr.value(), ty)) success = true;
        }
        break;
    case Oper::MUL:
        if (isTypeI) {
            if (assignBasedOnTypeI(evalVal, il.value()*ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(evalVal, ul.value()*ur.value(), ty)) success = true;
        } else if (isTypeF) {
            if (assignBasedOnTypeF(evalVal, fl.value()*fr.value(), ty)) success = true;
        }
        break;
    case Oper::DIV:
        if (isTypeI) {
            if (assignBasedOnTypeI(evalVal, il.value()/ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(evalVal, ul.value()/ur.value(), ty)) success = true;
        } else if (isTypeF) {
            if (assignBasedOnTypeF(evalVal, fl.value()/fr.value(), ty)) success = true;
        }
        break;
    case Oper::REM:
        if (isTypeI) {
            if (assignBasedOnTypeI(evalVal, il.value()%ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(evalVal, ul.value()%ur.value(), ty)) success = true;
        } else if (isTypeF) {
            if (typeTable->worksAsPrimitive(ty, TypeTable::P_F32)) {
                evalVal.f32 = fmod(lhs.getEvalVal().f32, rhs.getEvalVal().f32);
            } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_F64)) {
                evalVal.f64 = fmod(lhs.getEvalVal().f64, rhs.getEvalVal().f64);
            }
            success = true;
        }
        break;
    case Oper::SHL:
        if (isTypeI) {
            if (assignBasedOnTypeI(evalVal, il.value()<<ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(evalVal, ul.value()<<ur.value(), ty)) success = true;
        }
        break;
    case Oper::SHR:
        if (isTypeI) {
            if (assignBasedOnTypeI(evalVal, il.value()>>ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(evalVal, ul.value()>>ur.value(), ty)) success = true;
        }
        break;
    case Oper::BIT_AND:
        if (isTypeI) {
            if (assignBasedOnTypeI(evalVal, il.value()&ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(evalVal, ul.value()&ur.value(), ty)) success = true;
        }
        break;
    case Oper::BIT_OR:
        if (isTypeI) {
            if (assignBasedOnTypeI(evalVal, il.value()|ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(evalVal, ul.value()|ur.value(), ty)) success = true;
        }
        break;
    case Oper::BIT_XOR:
        if (isTypeI) {
            if (assignBasedOnTypeI(evalVal, il.value()^ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(evalVal, ul.value()^ur.value(), ty)) success = true;
        }
        break;
    }

    if (!success) {
        if (!errorGiven) msgs->errorExprEvalBinBadOp(codeLoc);
        return NodeVal();
    }

    return NodeVal(codeLoc, evalVal);
}

NodeVal Evaluator::performTuple(CodeLoc codeLoc, TypeTable::Id ty, const std::vector<NodeVal> &membs) {
    EvalVal evalVal = EvalVal::makeVal(ty, typeTable);

    for (size_t i = 0; i < membs.size(); ++i) {
        const NodeVal &memb = membs[i];
        if (!checkIsEvalVal(memb, true)) return NodeVal();
        
        evalVal.elems[i] = NodeVal(memb.getCodeLoc(), EvalVal::copyNoRef(memb.getEvalVal()));
    }

    return NodeVal(codeLoc, evalVal);
}

// keep in sync with EvalVal::isCastable
// TODO warn on lossy
optional<EvalVal> Evaluator::makeCast(const EvalVal &srcEvalVal, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId) {
    if (srcTypeId == dstTypeId) return EvalVal::copyNoRef(srcEvalVal);

    optional<EvalVal> dstEvalVal = EvalVal::makeVal(dstTypeId, typeTable);

    if (typeTable->worksAsTypeI(srcTypeId)) {
        int64_t x = EvalVal::getValueI(srcEvalVal, typeTable).value();
        if (!assignBasedOnTypeI(dstEvalVal.value(), (int64_t) x, dstTypeId) &&
            !assignBasedOnTypeU(dstEvalVal.value(), (uint64_t) x, dstTypeId) &&
            !assignBasedOnTypeF(dstEvalVal.value(), (double) x, dstTypeId) &&
            !assignBasedOnTypeC(dstEvalVal.value(), (char) x, dstTypeId) &&
            !assignBasedOnTypeB(dstEvalVal.value(), (bool) x, dstTypeId) &&
            !(typeTable->worksAsTypeStr(dstTypeId) && x == 0) &&
            !(typeTable->worksAsTypeAnyP(dstTypeId) && x == 0)) {
            dstEvalVal.reset();
        }
    } else if (typeTable->worksAsTypeU(srcTypeId)) {
        uint64_t x = EvalVal::getValueU(srcEvalVal, typeTable).value();
        if (!assignBasedOnTypeI(dstEvalVal.value(), (int64_t) x, dstTypeId) &&
            !assignBasedOnTypeU(dstEvalVal.value(), (uint64_t) x, dstTypeId) &&
            !assignBasedOnTypeF(dstEvalVal.value(), (double) x, dstTypeId) &&
            !assignBasedOnTypeC(dstEvalVal.value(), (char) x, dstTypeId) &&
            !assignBasedOnTypeB(dstEvalVal.value(), (bool) x, dstTypeId) &&
            !(typeTable->worksAsTypeStr(dstTypeId) && x == 0) &&
            !(typeTable->worksAsTypeAnyP(dstTypeId) && x == 0)) {
            dstEvalVal.reset();
        }
    } else if (typeTable->worksAsTypeF(srcTypeId)) {
        double x = EvalVal::getValueF(srcEvalVal, typeTable).value();
        if (!assignBasedOnTypeI(dstEvalVal.value(), (int64_t) x, dstTypeId) &&
            !assignBasedOnTypeU(dstEvalVal.value(), (uint64_t) x, dstTypeId) &&
            !assignBasedOnTypeF(dstEvalVal.value(), (double) x, dstTypeId)) {
            dstEvalVal.reset();
        }
    } else if (typeTable->worksAsTypeC(srcTypeId)) {
        if (!assignBasedOnTypeI(dstEvalVal.value(), (int64_t) srcEvalVal.c8, dstTypeId) &&
            !assignBasedOnTypeU(dstEvalVal.value(), (uint64_t) srcEvalVal.c8, dstTypeId) &&
            !assignBasedOnTypeC(dstEvalVal.value(), srcEvalVal.c8, dstTypeId) &&
            !assignBasedOnTypeB(dstEvalVal.value(), (bool) srcEvalVal.c8, dstTypeId)) {
            dstEvalVal.reset();
        }
    } else if (typeTable->worksAsTypeB(srcTypeId)) {
        if (!assignBasedOnTypeI(dstEvalVal.value(), srcEvalVal.b ? 1 : 0, dstTypeId) &&
            !assignBasedOnTypeU(dstEvalVal.value(), srcEvalVal.b ? 1 : 0, dstTypeId) &&
            !assignBasedOnTypeB(dstEvalVal.value(), srcEvalVal.b, dstTypeId)) {
            dstEvalVal.reset();
        }
    } else if (typeTable->worksAsTypeStr(srcTypeId)) {
        if (srcEvalVal.str.has_value()) {
            const string &str = stringPool->get(srcEvalVal.str.value());
            if (typeTable->worksAsTypeStr(dstTypeId)) {
                dstEvalVal.value().str = srcEvalVal.str;
            } else if (typeTable->worksAsTypeB(dstTypeId)) {
                dstEvalVal.value().b = true;
            } else if (typeTable->worksAsTypeCharArrOfLen(dstTypeId, LiteralVal::getStringLen(str))) {
                dstEvalVal = makeArray(dstTypeId);
                if (!dstEvalVal.has_value()) { 
                    dstEvalVal.reset();
                } else {
                    for (size_t i = 0; i < LiteralVal::getStringLen(str); ++i) {
                        dstEvalVal.value().elems[i].getEvalVal().c8 = str[i];
                    }
                }
            } else {
                dstEvalVal.reset();
            }
        } else {
            // if it's not string, it's null
            if (!assignBasedOnTypeI(dstEvalVal.value(), 0, dstTypeId) &&
                !assignBasedOnTypeU(dstEvalVal.value(), 0, dstTypeId) &&
                !assignBasedOnTypeB(dstEvalVal.value(), false, dstTypeId) &&
                !typeTable->worksAsTypeAnyP(dstTypeId)) {
                dstEvalVal.reset();
            }
        }
    } else if (typeTable->worksAsTypeAnyP(srcTypeId)) {
        // it's null
        if (!assignBasedOnTypeI(dstEvalVal.value(), 0, dstTypeId) &&
            !assignBasedOnTypeU(dstEvalVal.value(), 0, dstTypeId) &&
            !assignBasedOnTypeB(dstEvalVal.value(), false, dstTypeId) &&
            !typeTable->worksAsTypeAnyP(dstTypeId)) {
            dstEvalVal.reset();
        }
    } else if (typeTable->worksAsTypeArr(srcTypeId) ||
        typeTable->worksAsTuple(srcTypeId) ||
        typeTable->worksAsPrimitive(srcTypeId, TypeTable::P_ID) ||
        typeTable->worksAsPrimitive(srcTypeId, TypeTable::P_TYPE) ||
        typeTable->worksAsPrimitive(srcTypeId, TypeTable::P_RAW)) {
        // these types are only castable when changing constness
        if (typeTable->isImplicitCastable(srcTypeId, dstTypeId)) {
            // no action is needed in case of a cast
            // except for raw when dropping cn
            dstEvalVal = EvalVal::copyNoRef(srcEvalVal);
            dstEvalVal.value().type = dstTypeId;

            if (EvalVal::isRaw(dstEvalVal.value(), typeTable) &&
                !typeTable->worksAsTypeCn(dstTypeId) && typeTable->worksAsTypeCn(srcTypeId)) {
                EvalVal::equalizeAllRawElemTypes(dstEvalVal.value(), typeTable);
            }
        } else {
            dstEvalVal.reset();
        }
    } else {
        dstEvalVal.reset();
    }

    return dstEvalVal;
}

optional<EvalVal> Evaluator::makeArray(TypeTable::Id arrTypeId) {
    if (!typeTable->worksAsTypeArr(arrTypeId)) return nullopt;

    return EvalVal::makeVal(arrTypeId, typeTable);
}

vector<NodeVal> Evaluator::makeRawConcat(const EvalVal &lhs, const EvalVal &rhs) const {
    vector<NodeVal> elems;
    elems.reserve(lhs.elems.size()+rhs.elems.size());

    for (const auto &it : lhs.elems) {
        elems.push_back(it);
    }
    for (const auto &it : rhs.elems) {
        elems.push_back(it);
    }

    return elems;
}

bool Evaluator::assignBasedOnTypeI(EvalVal &val, int64_t x, TypeTable::Id ty) {
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

bool Evaluator::assignBasedOnTypeU(EvalVal &val, uint64_t x, TypeTable::Id ty) {
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

bool Evaluator::assignBasedOnTypeF(EvalVal &val, double x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_F32)) {
        val.f32 = (float) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_F64)) {
        val.f64 = (double) x;
    } else {
        return false;
    }

    return true;
}

bool Evaluator::assignBasedOnTypeC(EvalVal &val, char x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_C8)) {
        val.c8 = x;
    } else {
        return false;
    }

    return true;
}

bool Evaluator::assignBasedOnTypeB(EvalVal &val, bool x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_BOOL)) {
        val.b = x;
    } else {
        return false;
    }

    return true;
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