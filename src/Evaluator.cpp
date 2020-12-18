#include "Evaluator.h"
#include <sstream>
#include "exceptions.h"
using namespace std;

struct ComparisonSignal {
    bool result = true;
};

Evaluator::Evaluator(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompilationMessages *msgs)
    : Processor(namePool, stringPool, typeTable, symbolTable, msgs) {
    setEvaluator(this);
}

NodeVal Evaluator::performLoad(CodeLoc codeLoc, NamePool::Id id, NodeVal &ref) {
    if (checkIsEvalVal(codeLoc, ref, false)) {
        NodeVal nodeVal = NodeVal::copyNoRef(codeLoc, ref);
        nodeVal.getEvalVal().ref = &ref;
        return nodeVal;
    } else {
        // allowing eval to load compiled variables risks fetching outdated values from symbol table
        // however, eval needs to be able to do this for macros to be able to work with compiled args
        // TODO find a way to limit this behaviour to make it correct
        // maybe allow to load invoke args and their attrs only
        return ref;
    }
}

NodeVal Evaluator::performZero(CodeLoc codeLoc, TypeTable::Id ty) {
    return NodeVal(codeLoc, EvalVal::makeZero(ty, namePool, typeTable));
}

// not used right now, because of mandatory zero init
// once ::noZero is introduced, will be reachable again
// then, make sure to still zero init ids and types (to avoid segfaults)
NodeVal Evaluator::performRegister(CodeLoc codeLoc, NamePool::Id id, TypeTable::Id ty) {
    return NodeVal(codeLoc, EvalVal::makeVal(ty, typeTable));
}

NodeVal Evaluator::performRegister(CodeLoc codeLoc, NamePool::Id id, const NodeVal &init) {
    if (!checkIsEvalVal(init, true)) return NodeVal();

    return NodeVal::copyNoRef(codeLoc, init);
}

NodeVal Evaluator::performCast(CodeLoc codeLoc, const NodeVal &node, TypeTable::Id ty) {
    if (!checkIsEvalVal(node, true) || !checkHasType(node, true)) return NodeVal();

    optional<NodeVal> evalValCast = makeCast(codeLoc, node, node.getEvalVal().type.value(), ty);
    if (!evalValCast.has_value()) {
        if (EvalVal::isImplicitCastable(node.getEvalVal(), ty, stringPool, typeTable)) {
            // if it's implicitly castable, it should be castable
            msgs->errorInternal(codeLoc);
        } else {
            msgs->errorExprCannotCast(codeLoc, node.getEvalVal().type.value(), ty);
        }
        return NodeVal();
    }

    return evalValCast.value();
}

optional<bool> Evaluator::performBlockBody(CodeLoc codeLoc, const SymbolTable::Block &block, const NodeVal &nodeBody) {
    try {
        bool bodySuccess = processChildNodes(nodeBody);
        if (!bodySuccess) return nullopt;
        return false;
    } catch (ExceptionEvaluatorJump ex) {
        bool forCurrBlock = !ex.isRet && (!ex.blockName.has_value() || ex.blockName == block.name);
        if (!forCurrBlock) {
            doBlockTearDown(codeLoc, block, true, true);
            throw ex;
        }

        return ex.isLoop;
    }
}

NodeVal Evaluator::performBlockTearDown(CodeLoc codeLoc, const SymbolTable::Block &block, bool success) {
    return doBlockTearDown(codeLoc, block, success, false);
}

bool Evaluator::performExit(CodeLoc codeLoc, const SymbolTable::Block &block, const NodeVal &cond) {
    if (!checkIsEvalVal(cond, true)) return false;
    if (!block.isEval()) {
        msgs->errorEvaluationNotSupported(codeLoc);
        return false;
    }

    if (cond.getEvalVal().b) {
        ExceptionEvaluatorJump ex;
        // if name not given, skip until innermost block (instruction can't do otherwise)
        // if name given, skip until that block (which may even be innermost block)
        ex.blockName = block.name;
        throw ex;
    }

    return true;
}

bool Evaluator::performLoop(CodeLoc codeLoc, const SymbolTable::Block &block, const NodeVal &cond) {
    if (!checkIsEvalVal(cond, true)) return false;
    if (!block.isEval()) {
        msgs->errorEvaluationNotSupported(codeLoc);
        return false;
    }

    if (cond.getEvalVal().b) {
        ExceptionEvaluatorJump ex;
        // if name not given, skip until innermost block (instruction can't do otherwise)
        // if name given, skip until that block (which may even be innermost block)
        ex.blockName = block.name;
        ex.isLoop = true;
        throw ex;
    }

    return true;
}

bool Evaluator::performPass(CodeLoc codeLoc, SymbolTable::Block &block, const NodeVal &val) {
    if (!checkIsEvalVal(val, true)) return false;
    if (!block.isEval()) {
        msgs->errorEvaluationNotSupported(codeLoc);
        return false;
    }

    block.val = NodeVal::copyNoRef(codeLoc, val);

    ExceptionEvaluatorJump ex;
    ex.blockName = block.name;
    throw ex;
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

    try {
        if (!processChildNodes(func.evalFunc.value())) {
            return NodeVal();
        }
    } catch (ExceptionEvaluatorJump ex) {
        if (!ex.isRet) {
            msgs->errorInternal(codeLoc);
            return NodeVal();
        }
    }

    if (func.hasRet()) {
        if (!retVal.has_value()) {
            msgs->errorRetNoValue(codeLoc, func.retType.value());
            return NodeVal();
        }

        return move(retVal.value());
    } else {
        return NodeVal(codeLoc);
    }
}

NodeVal Evaluator::performInvoke(CodeLoc codeLoc, const MacroValue &macro, const std::vector<NodeVal> &args) {
    BlockControl blockCtrl(symbolTable, macro);

    for (size_t i = 0; i < args.size(); ++i) {
        symbolTable->addVar(macro.argNames[i], args[i]);
    }

    try {
        if (!processChildNodes(macro.body)) {
            return NodeVal();
        }
    } catch (ExceptionEvaluatorJump ex) {
        if (!ex.isRet) {
            msgs->errorInternal(codeLoc);
            return NodeVal();
        }
    }

    if (!retVal.has_value()) {
        msgs->errorUnknown(codeLoc);
        return NodeVal();
    }

    return move(retVal.value());
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

bool Evaluator::performMacroDefinition(const NodeVal &args, const NodeVal &body, MacroValue &macro) {
    macro.body = body;
    return true;
}

bool Evaluator::performRet(CodeLoc codeLoc) {
    if (symbolTable->getCurrFunc().has_value() && !symbolTable->getCurrFunc().value().isEval()) {
        msgs->errorEvaluationNotSupported(codeLoc);
        return false;
    }

    ExceptionEvaluatorJump ex;
    ex.isRet = true;
    throw ex;
}

bool Evaluator::performRet(CodeLoc codeLoc, const NodeVal &node) {
    retVal = node;
    performRet(codeLoc);

    return false; // unreachable
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
        if (oper.hasRef()) {
            evalVal = EvalVal::makeVal(typeTable->addTypeAddrOf(ty), typeTable);
            evalVal.p = oper.getEvalVal().ref;
            success = true;
        }
    }

    if (!success) {
        if (!errorGiven) msgs->errorExprUnBadType(codeLoc);
        return NodeVal();
    }

    return NodeVal(codeLoc, evalVal);
}

NodeVal Evaluator::performOperUnaryDeref(CodeLoc codeLoc, const NodeVal &oper, TypeTable::Id resTy) {
    if (!checkIsEvalVal(oper, true)) return NodeVal();

    if (EvalVal::isNull(oper.getEvalVal(), typeTable)) {
        msgs->errorUnknown(oper.getCodeLoc());
        return NodeVal();
    }

    NodeVal nodeEvalVal = NodeVal::copyNoRef(*oper.getEvalVal().p);
    nodeEvalVal.getEvalVal().type = resTy;
    nodeEvalVal.getEvalVal().ref = oper.getEvalVal().p;
    return nodeEvalVal;
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
    bool isTypeP = typeTable->worksAsTypeP(ty);
    bool isTypeAnyP = typeTable->worksAsTypeAnyP(ty);
    bool isTypeB = typeTable->worksAsTypeB(ty);
    bool isTypeId = typeTable->worksAsPrimitive(ty, TypeTable::P_ID);
    bool isTypeTy = typeTable->worksAsPrimitive(ty, TypeTable::P_TYPE);

    optional<int64_t> il, ir;
    optional<uint64_t> ul, ur;
    optional<char> cl, cr;
    optional<double> fl, fr;
    optional<StringPool::Id> strl, strr;
    optional<const NodeVal*> pl, pr;
    optional<bool> bl, br;
    optional<NamePool::Id> idl, idr;
    optional<TypeTable::Id> tyl, tyr;
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
    } else if (isTypeAnyP) {
        if (isTypeP) {
            pl = lhs.getEvalVal().p;
            pr = rhs.getEvalVal().p;
        } else {
            pl = pr = nullptr;
        }
    } else if (isTypeB) {
        bl = lhs.getEvalVal().b;
        br = rhs.getEvalVal().b;
    } else if (isTypeId) {
        idl = lhs.getEvalVal().id;
        idr = rhs.getEvalVal().id;
    } else if (isTypeTy) {
        tyl = lhs.getEvalVal().ty;
        tyr = rhs.getEvalVal().ty;
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
        } else if (isTypeAnyP) {
            result->result = pl==pr;
            return !result->result;
        } else if (isTypeB) {
            result->result = bl.value()==br.value();
            return !result->result;
        } else if (isTypeId) {
            result->result = idl.value()==idr.value();
            return !result->result;
        } else if (isTypeTy) {
            result->result = tyl.value()==tyr.value();
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
        } else if (isTypeAnyP) {
            result->result = pl!=pr;
            return !result->result;
        } else if (isTypeB) {
            result->result = bl.value()!=br.value();
            return !result->result;
        } else if (isTypeId) {
            result->result = idl.value()!=idr.value();
            return !result->result;
        } else if (isTypeTy) {
            result->result = tyl.value()!=tyr.value();
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

// if processing an operand threw ExceptionEvaluatorJump, teardown won't get called
NodeVal Evaluator::performOperComparisonTearDown(CodeLoc codeLoc, bool success, void *signal) {
    if (!success) return NodeVal();

    EvalVal evalVal = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_BOOL), typeTable);
    evalVal.b = ((ComparisonSignal*) signal)->result;
    return NodeVal(codeLoc, evalVal);
}

NodeVal Evaluator::performOperAssignment(CodeLoc codeLoc, NodeVal &lhs, const NodeVal &rhs) {
    if (!checkIsEvalVal(lhs, true) || !checkIsEvalVal(rhs, true)) return NodeVal();

    *lhs.getEvalVal().ref = NodeVal::copyNoRef(lhs.getEvalVal().ref->getCodeLoc(), rhs);

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

    if (typeTable->worksAsTypeArr(base.getType().value())) {
        NodeVal nodeVal = NodeVal::copyNoRef(codeLoc, base.getEvalVal().elems[index.value()]);
        if (base.hasRef()) {
            nodeVal.getEvalVal().ref = &base.getEvalVal().ref->getEvalVal().elems[index.value()];
        }
        return nodeVal;
    } else if (typeTable->worksAsTypeArrP(base.getType().value())) {
        msgs->errorUnknown(codeLoc);
        return NodeVal();
    } else {
        msgs->errorInternal(codeLoc);
        return NodeVal();
    }
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
        NodeVal nodeVal = NodeVal::copyNoRef(codeLoc, base.getEvalVal().elems[ind]);
        nodeVal.getEvalVal().type = resTy;
        if (base.hasRef()) {
            nodeVal.getEvalVal().ref = &base.getEvalVal().ref->getEvalVal().elems[ind];
        }
        return nodeVal;
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
    bool isTypeId = typeTable->worksAsPrimitive(ty, TypeTable::P_ID);
    bool isTypeRaw = NodeVal::isRawVal(lhs, typeTable);

    optional<int64_t> il, ir;
    optional<uint64_t> ul, ur;
    optional<double> fl, fr;
    optional<NamePool::Id> idl, idr;
    if (isTypeI) {
        il = EvalVal::getValueI(lhs.getEvalVal(), typeTable).value();
        ir = EvalVal::getValueI(rhs.getEvalVal(), typeTable).value();
    } else if (isTypeU) {
        ul = EvalVal::getValueU(lhs.getEvalVal(), typeTable).value();
        ur = EvalVal::getValueU(rhs.getEvalVal(), typeTable).value();
    } else if (isTypeF) {
        fl = EvalVal::getValueF(lhs.getEvalVal(), typeTable).value();
        fr = EvalVal::getValueF(rhs.getEvalVal(), typeTable).value();
    } else if (isTypeId) {
        idl = lhs.getEvalVal().id;
        idr = rhs.getEvalVal().id;
    }

    switch (op) {
    case Oper::ADD:
        if (isTypeI) {
            if (assignBasedOnTypeI(evalVal, il.value()+ir.value(), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(evalVal, ul.value()+ur.value(), ty)) success = true;
        } else if (isTypeF) {
            if (assignBasedOnTypeF(evalVal, fl.value()+fr.value(), ty)) success = true;
        } else if (isTypeId) {
            evalVal.id = makeIdConcat(lhs.getEvalVal().id, rhs.getEvalVal().id);
            success = true;
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
        
        evalVal.elems[i] = NodeVal::copyNoRef(memb.getCodeLoc(), memb);
    }

    return NodeVal(codeLoc, evalVal);
}

optional<uint64_t> Evaluator::performSizeOf(CodeLoc codeLoc, TypeTable::Id ty) {
    msgs->errorInternal(codeLoc);
    return nullopt;
}

NodeVal Evaluator::doBlockTearDown(CodeLoc codeLoc, const SymbolTable::Block &block, bool success, bool jumpingOut) {
    if (!success) return NodeVal();

    if (!jumpingOut && block.type.has_value()) {
        if (!block.val.has_value()) {
            msgs->errorBlockNoPass(codeLoc);
            return NodeVal();
        }

        return block.val.value();
    }

    return NodeVal(codeLoc);
}

// keep in sync with EvalVal::isCastable
// TODO warn on lossy
optional<NodeVal> Evaluator::makeCast(CodeLoc codeLoc, const NodeVal &srcVal, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId) {
    if (srcTypeId == dstTypeId) return NodeVal::copyNoRef(codeLoc, srcVal);

    const EvalVal &srcEvalVal = srcVal.getEvalVal();
    EvalVal dstEvalVal = EvalVal::makeVal(dstTypeId, typeTable);

    if (typeTable->worksAsTypeI(srcTypeId)) {
        int64_t x = EvalVal::getValueI(srcEvalVal, typeTable).value();
        if (!assignBasedOnTypeI(dstEvalVal, (int64_t) x, dstTypeId) &&
            !assignBasedOnTypeU(dstEvalVal, (uint64_t) x, dstTypeId) &&
            !assignBasedOnTypeF(dstEvalVal, (double) x, dstTypeId) &&
            !assignBasedOnTypeC(dstEvalVal, (char) x, dstTypeId) &&
            !assignBasedOnTypeB(dstEvalVal, (bool) x, dstTypeId) &&
            !(x == 0 && assignBasedOnTypeP(dstEvalVal, nullptr, dstTypeId)) &&
            !(x == 0 && typeTable->worksAsTypeStr(dstTypeId)) &&
            !(x == 0 && typeTable->worksAsTypeAnyP(dstTypeId))) {
            return nullopt;
        }
    } else if (typeTable->worksAsTypeU(srcTypeId)) {
        uint64_t x = EvalVal::getValueU(srcEvalVal, typeTable).value();
        if (!assignBasedOnTypeI(dstEvalVal, (int64_t) x, dstTypeId) &&
            !assignBasedOnTypeU(dstEvalVal, (uint64_t) x, dstTypeId) &&
            !assignBasedOnTypeF(dstEvalVal, (double) x, dstTypeId) &&
            !assignBasedOnTypeC(dstEvalVal, (char) x, dstTypeId) &&
            !assignBasedOnTypeB(dstEvalVal, (bool) x, dstTypeId) &&
            !(x == 0 && assignBasedOnTypeP(dstEvalVal, nullptr, dstTypeId)) &&
            !(x == 0 && typeTable->worksAsTypeStr(dstTypeId)) &&
            !(x == 0 && typeTable->worksAsTypeAnyP(dstTypeId)) &&
            !assignBasedOnTypeId(dstEvalVal, x, dstTypeId)) {
            return nullopt;
        }
    } else if (typeTable->worksAsTypeF(srcTypeId)) {
        double x = EvalVal::getValueF(srcEvalVal, typeTable).value();
        if (!assignBasedOnTypeI(dstEvalVal, (int64_t) x, dstTypeId) &&
            !assignBasedOnTypeU(dstEvalVal, (uint64_t) x, dstTypeId) &&
            !assignBasedOnTypeF(dstEvalVal, (double) x, dstTypeId)) {
            return nullopt;
        }
    } else if (typeTable->worksAsTypeC(srcTypeId)) {
        if (!assignBasedOnTypeI(dstEvalVal, (int64_t) srcEvalVal.c8, dstTypeId) &&
            !assignBasedOnTypeU(dstEvalVal, (uint64_t) srcEvalVal.c8, dstTypeId) &&
            !assignBasedOnTypeC(dstEvalVal, srcEvalVal.c8, dstTypeId) &&
            !assignBasedOnTypeB(dstEvalVal, (bool) srcEvalVal.c8, dstTypeId)) {
            return nullopt;
        }
    } else if (typeTable->worksAsTypeB(srcTypeId)) {
        if (!assignBasedOnTypeI(dstEvalVal, srcEvalVal.b ? 1 : 0, dstTypeId) &&
            !assignBasedOnTypeU(dstEvalVal, srcEvalVal.b ? 1 : 0, dstTypeId) &&
            !assignBasedOnTypeB(dstEvalVal, srcEvalVal.b, dstTypeId) &&
            !assignBasedOnTypeId(dstEvalVal, srcEvalVal.b, dstTypeId)) {
            return nullopt;
        }
    } else if (typeTable->worksAsTypeStr(srcTypeId)) {
        if (srcEvalVal.str.has_value()) {
            const string &str = stringPool->get(srcEvalVal.str.value());
            if (typeTable->worksAsTypeStr(dstTypeId)) {
                dstEvalVal.str = srcEvalVal.str;
            } else if (typeTable->worksAsTypeB(dstTypeId)) {
                dstEvalVal.b = true;
            } else if (typeTable->worksAsTypeCharArrOfLen(dstTypeId, LiteralVal::getStringLen(str))) {
                optional<EvalVal> arrEvalVal = makeArray(dstTypeId);
                if (!arrEvalVal.has_value()) { 
                    return nullopt;
                } else {
                    dstEvalVal = arrEvalVal.value();
                    for (size_t i = 0; i < LiteralVal::getStringLen(str); ++i) {
                        dstEvalVal.elems[i].getEvalVal().c8 = str[i];
                    }
                }
            } else {
                return nullopt;
            }
        } else {
            // if it's not string, it's null
            if (!assignBasedOnTypeI(dstEvalVal, 0, dstTypeId) &&
                !assignBasedOnTypeU(dstEvalVal, 0, dstTypeId) &&
                !assignBasedOnTypeB(dstEvalVal, false, dstTypeId) &&
                !assignBasedOnTypeP(dstEvalVal, nullptr, dstTypeId) &&
                !typeTable->worksAsTypeAnyP(dstTypeId)) {
                return nullopt;
            }
        }
    } else if (typeTable->worksAsTypeAnyP(srcTypeId)) {
        if (EvalVal::isNull(srcEvalVal, typeTable)) {
            if (!assignBasedOnTypeI(dstEvalVal, 0, dstTypeId) &&
                !assignBasedOnTypeU(dstEvalVal, 0, dstTypeId) &&
                !assignBasedOnTypeB(dstEvalVal, false, dstTypeId) &&
                !assignBasedOnTypeP(dstEvalVal, nullptr, dstTypeId) &&
                !typeTable->worksAsTypeAnyP(dstTypeId)) {
                return nullopt;
            }
        } else {
            if (typeTable->isImplicitCastable(srcTypeId, dstTypeId)) {
                if (!assignBasedOnTypeP(dstEvalVal, srcEvalVal.p, dstTypeId)) {
                    return nullopt;
                }
            } else {
                return nullopt;
            }
        }
    } else if (typeTable->worksAsPrimitive(srcTypeId, TypeTable::P_TYPE)) {
        if (typeTable->worksAsPrimitive(dstTypeId, TypeTable::P_TYPE)) {
            dstEvalVal = EvalVal::copyNoRef(srcEvalVal);
            dstEvalVal.type = dstTypeId;
        } else if (typeTable->worksAsPrimitive(dstTypeId, TypeTable::P_ID)) {
            optional<NamePool::Id> id = makeIdFromTy(srcEvalVal.ty);
            if (id.has_value()) dstEvalVal.id = id.value();
            else return nullopt;
        } else {
            return nullopt;
        }
    } else if (typeTable->worksAsTypeArr(srcTypeId) ||
        typeTable->worksAsTuple(srcTypeId) ||
        typeTable->worksAsPrimitive(srcTypeId, TypeTable::P_ID) ||
        typeTable->worksAsPrimitive(srcTypeId, TypeTable::P_RAW)) {
        // these types are only castable when changing constness
        if (typeTable->isImplicitCastable(srcTypeId, dstTypeId)) {
            // no action is needed in case of a cast
            // except for raw when dropping cn
            dstEvalVal = EvalVal::copyNoRef(srcEvalVal);
            dstEvalVal.type = dstTypeId;

            if (EvalVal::isRaw(dstEvalVal, typeTable) &&
                !typeTable->worksAsTypeCn(dstTypeId) && typeTable->worksAsTypeCn(srcTypeId)) {
                EvalVal::equalizeAllRawElemTypes(dstEvalVal, typeTable);
            }
        } else {
            return nullopt;
        }
    } else {
        return nullopt;
    }

    return NodeVal(codeLoc, dstEvalVal);
}

optional<EvalVal> Evaluator::makeArray(TypeTable::Id arrTypeId) {
    if (!typeTable->worksAsTypeArr(arrTypeId)) return nullopt;

    return EvalVal::makeVal(arrTypeId, typeTable);
}

NamePool::Id Evaluator::makeIdFromU(std::uint64_t x) {
    stringstream ss;
    ss << "$U" << x;
    return namePool->add(ss.str());
}

NamePool::Id Evaluator::makeIdFromB(bool x) {
    return namePool->add(x ? "$Bt" : "$Bf");
}

optional<NamePool::Id> Evaluator::makeIdFromTy(TypeTable::Id x) {
    optional<string> str = typeTable->makeBinString(x);
    if (!str.has_value()) return nullopt;

    stringstream ss;
    ss << "$T" << str.value();
    return namePool->add(ss.str());
}

NamePool::Id Evaluator::makeIdConcat(NamePool::Id lhs, NamePool::Id rhs) {
    stringstream ss;
    ss << namePool->get(lhs) << "$+" << namePool->get(rhs);
    return namePool->add(ss.str());
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

bool Evaluator::assignBasedOnTypeP(EvalVal &val, NodeVal *x, TypeTable::Id ty) {
    if (typeTable->worksAsTypeP(ty)) {
        val.p = x;
    } else {
        return false;
    }

    return true;
}

bool Evaluator::assignBasedOnTypeId(EvalVal &val, std::uint64_t x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_ID)) {
        val.id = makeIdFromU(x);
    } else {
        return false;
    }

    return true;
}

bool Evaluator::assignBasedOnTypeId(EvalVal &val, bool x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_ID)) {
        val.id = makeIdFromB(x);
    } else {
        return false;
    }

    return true;
}