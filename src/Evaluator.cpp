#include "Evaluator.h"
#include <sstream>
#include "BlockRaii.h"
#include "exceptions.h"
#include "utils.h"
using namespace std;

Evaluator::Evaluator(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompilationMessages *msgs)
    : Processor(namePool, stringPool, typeTable, symbolTable, msgs) {
    setEvaluator(this);
}

NodeVal Evaluator::performLoad(CodeLoc codeLoc, VarId varId) {
    SymbolTable::VarEntry &ref = symbolTable->getVar(varId);

    if (checkIsEvalVal(codeLoc, ref.var, false)) {
        if (ref.var.isInvokeArg()) {
            return ref.var;
        } else {
            NodeVal nodeVal = NodeVal::copyNoRef(codeLoc, ref.var);
            nodeVal.getEvalVal().getRef() = varId;
            return nodeVal;
        }
    } else {
        // compiled values may only be loaded if they are invoke arguments
        // for generality, don't check specifically if it's LlvmVal
        if (ref.var.getLifetimeInfo().has_value() && !ref.var.isInvokeArg()) {
            msgs->errorNotEvalVal(codeLoc);
            return NodeVal();
        }
        return ref.var;
    }
}

NodeVal Evaluator::performLoad(CodeLoc codeLoc, FuncId funcId) {
    const FuncValue &func = symbolTable->getFunc(funcId);

    if (!checkIsEvalFunc(codeLoc, func, true)) return NodeVal();

    EvalVal evalVal = EvalVal::makeVal(func.getType(), typeTable);
    evalVal.f() = funcId;
    return NodeVal(codeLoc, evalVal);
}

NodeVal Evaluator::performLoad(CodeLoc codeLoc, MacroId macroId) {
    EvalVal evalVal = EvalVal::makeVal(symbolTable->getMacro(macroId).getType(), typeTable);
    evalVal.m() = macroId;
    return NodeVal(codeLoc, evalVal);
}

// id and type vals must contain valid NamePool/TypeTable indexes
// because of this, ::noZero is ignored on EvalVals
NodeVal Evaluator::performZero(CodeLoc codeLoc, TypeTable::Id ty) {
    return NodeVal(codeLoc, EvalVal::makeZero(ty, namePool, typeTable));
}

NodeVal Evaluator::performRegister(CodeLoc codeLoc, NamePool::Id id, CodeLoc codeLocTy, TypeTable::Id ty) {
    EvalVal evalVal = EvalVal::makeZero(ty, namePool, typeTable);
    evalVal.getLifetimeInfo().nestLevel = symbolTable->currNestLevel();
    return NodeVal(codeLoc, move(evalVal));
}

NodeVal Evaluator::performRegister(CodeLoc codeLoc, NamePool::Id id, NodeVal init) {
    if (!checkIsEvalVal(init, true)) return NodeVal();

    LifetimeInfo lifetimeInfo;
    lifetimeInfo.nestLevel = symbolTable->currNestLevel();

    return NodeVal::moveNoRef(codeLoc, move(init), lifetimeInfo);
}

NodeVal Evaluator::performCast(CodeLoc codeLoc, const NodeVal &node, CodeLoc codeLocTy, TypeTable::Id ty) {
    if (!checkIsEvalVal(node, true)) return NodeVal();

    optional<NodeVal> evalValCast = makeCast(codeLoc, node, node.getEvalVal().getType(), ty);
    if (!evalValCast.has_value()) {
        msgs->errorExprCannotCast(codeLoc, node.getEvalVal().getType(), ty);
        return NodeVal();
    }
    if (node.hasRef()) evalValCast.value().setNoDrop(true);

    return move(evalValCast.value());
}

optional<bool> Evaluator::performBlockBody(CodeLoc codeLoc, SymbolTable::Block block, const NodeVal &nodeBody) {
    try {
        if (!processChildNodes(nodeBody)) return nullopt;

        if (!callDropFuncsCurrBlock(codeLoc)) return nullopt;

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

NodeVal Evaluator::performBlockTearDown(CodeLoc codeLoc, SymbolTable::Block block, bool success) {
    return doBlockTearDown(codeLoc, block, success, false);
}

bool Evaluator::performExit(CodeLoc codeLoc, SymbolTable::Block block, const NodeVal &cond) {
    if (!checkIsEvalVal(cond, true)) return false;
    if (!checkIsEvalBlock(codeLoc, block, true)) return false;

    if (cond.getEvalVal().b()) {
        if (!callDropFuncsFromBlockToCurrBlock(codeLoc, block.name)) return false;

        ExceptionEvaluatorJump ex;
        // if name not given, skip until innermost block (instruction can't do otherwise)
        // if name given, skip until that block (which may even be innermost block)
        ex.blockName = block.name;
        throw ex;
    }

    return true;
}

bool Evaluator::performLoop(CodeLoc codeLoc, SymbolTable::Block block, const NodeVal &cond) {
    if (!checkIsEvalVal(cond, true)) return false;
    if (!checkIsEvalBlock(codeLoc, block, true)) return false;

    if (cond.getEvalVal().b()) {
        if (!callDropFuncsFromBlockToCurrBlock(codeLoc, block.name)) return false;

        ExceptionEvaluatorJump ex;
        // if name not given, skip until innermost block (instruction can't do otherwise)
        // if name given, skip until that block (which may even be innermost block)
        ex.blockName = block.name;
        ex.isLoop = true;
        throw ex;
    }

    return true;
}

bool Evaluator::performPass(CodeLoc codeLoc, SymbolTable::Block block, NodeVal val) {
    if (!checkIsEvalVal(val, true)) return false;
    if (!checkIsEvalBlock(codeLoc, block, true)) return false;

    if (!callDropFuncsFromBlockToCurrBlock(codeLoc, block.name)) return false;

    retVal = NodeVal::moveNoRef(codeLoc, move(val), LifetimeInfo());

    ExceptionEvaluatorJump ex;
    ex.blockName = block.name;
    throw ex;
}

NodeVal Evaluator::performCall(CodeLoc codeLoc, CodeLoc codeLocFunc, const NodeVal &func, const std::vector<NodeVal> &args) {
    if (!checkIsEvalVal(func, true)) return NodeVal();
    if (!func.getEvalVal().f().has_value()) {
        msgs->errorFuncNoValue(codeLocFunc);
        return NodeVal();
    }

    return performCall(codeLoc, codeLocFunc, *func.getEvalVal().f(), args);
}

NodeVal Evaluator::performCall(CodeLoc codeLoc, CodeLoc codeLocFunc, FuncId funcId, const std::vector<NodeVal> &args) {
    const FuncValue &func = symbolTable->getFunc(funcId);

    if (!checkIsEvalFunc(codeLocFunc, func, true)) return NodeVal();

    for (const NodeVal &arg : args) {
        if (!checkIsEvalVal(arg, true)) return NodeVal();
    }

    if (!func.defined) {
        msgs->errorFuncNoDef(codeLocFunc);
        return NodeVal();
    }
    if (!func.isEvalFunc || func.evalFunc == nullptr || func.evalFunc->isInvalid()) {
        msgs->errorInternal(codeLocFunc);
        return NodeVal();
    }

    BlockRaii blockRaii(symbolTable, SymbolTable::CalleeValueInfo::make(func, typeTable));

    TypeTable::Callable callable = FuncValue::getCallable(func, typeTable);

    for (size_t i = 0; i < args.size(); ++i) {
        LifetimeInfo lifetimeInfo;
        lifetimeInfo.noDrop = callable.getArgNoDrop(i);
        lifetimeInfo.nestLevel = symbolTable->currNestLevel();

        SymbolTable::VarEntry varEntry;
        varEntry.name = func.argNames[i];
        varEntry.var = NodeVal::copyNoRef(args[i], lifetimeInfo);
        symbolTable->addVar(move(varEntry));
    }

    bool retIssued = false;
    try {
        if (!processChildNodes(*func.evalFunc)) {
            return NodeVal();
        }
    } catch (ExceptionEvaluatorJump ex) {
        if (!ex.isRet) {
            msgs->errorInternal(codeLoc);
            return NodeVal();
        }
        retIssued = true;
    }

    if (!retIssued && !callDropFuncsCurrCallable(codeLoc)) return NodeVal();

    if (callable.hasRet()) {
        if (!retVal.has_value()) {
            msgs->errorRetNoValue(codeLoc, callable.retType.value());
            return NodeVal();
        }

        NodeVal ret = NodeVal::moveNoRef(codeLoc, move(retVal.value()), LifetimeInfo());
        retVal.reset();
        return ret;
    } else {
        return NodeVal(codeLoc);
    }
}

NodeVal Evaluator::performInvoke(CodeLoc codeLoc, MacroId macroId, std::vector<NodeVal> args) {
    const MacroValue &macro = symbolTable->getMacro(macroId);

    LifetimeInfo::NestLevel nestLevel = symbolTable->currNestLevel();

    BlockRaii blockRaii(symbolTable, SymbolTable::CalleeValueInfo::make(macro));

    for (size_t i = 0; i < args.size(); ++i) {
        SymbolTable::VarEntry varEntry;
        varEntry.name = macro.argNames[i];
        varEntry.var = move(args[i]);

        if (varEntry.var.getLifetimeInfo().has_value()) {
            LifetimeInfo lifetimeInfo = varEntry.var.getLifetimeInfo().value();
            lifetimeInfo.invokeArg = true;
            varEntry.var.setLifetimeInfo(lifetimeInfo);
        }

        symbolTable->addVar(move(varEntry));
    }

    try {
        if (!processChildNodes(*macro.body)) {
            return NodeVal();
        }
    } catch (ExceptionEvaluatorJump ex) {
        if (!ex.isRet) {
            msgs->errorInternal(codeLoc);
            return NodeVal();
        }
    }

    if (!callDropFuncsCurrCallable(codeLoc)) return NodeVal();

    if (!retVal.has_value()) {
        msgs->errorMacroNoRet(codeLoc);
        return NodeVal();
    }

    NodeVal ret = move(retVal.value());
    retVal.reset();
    ret.setCodeLoc(codeLoc);
    return move(ret);
}

bool Evaluator::performFunctionDeclaration(CodeLoc codeLoc, FuncValue &func) {
    // mark the func as eval, but it cannot be called
    func.isEvalFunc = true;
    return true;
}

bool Evaluator::performFunctionDefinition(CodeLoc codeLoc, const NodeVal &args, const NodeVal &body, FuncValue &func) {
    func.evalFunc = std::make_unique<NodeVal>(body);
    return true;
}

bool Evaluator::performMacroDefinition(CodeLoc codeLoc, const NodeVal &args, const NodeVal &body, MacroValue &macro) {
    macro.body = std::make_unique<NodeVal>(body);
    return true;
}

bool Evaluator::performRet(CodeLoc codeLoc) {
    optional<SymbolTable::CalleeValueInfo> callee = symbolTable->getCurrCallee();
    if (!callee.value().isEval) {
        msgs->errorRetNonEval(codeLoc);
        return false;
    }

    if (!callDropFuncsCurrCallable(codeLoc)) return false;

    ExceptionEvaluatorJump ex;
    ex.isRet = true;
    throw ex;
}

bool Evaluator::performRet(CodeLoc codeLoc, NodeVal node) {
    optional<SymbolTable::CalleeValueInfo> callee = symbolTable->getCurrCallee();
    if (!callee.value().isEval) {
        msgs->errorRetNonEval(codeLoc);
        return false;
    }

    if (callee.value().isFunc) {
        retVal = NodeVal::moveNoRef(move(node), LifetimeInfo());
    } else {
        retVal = move(node);
        NodeVal::clearInvokeArg(retVal.value(), typeTable);
    }

    if (!callDropFuncsCurrCallable(codeLoc)) return false;

    ExceptionEvaluatorJump ex;
    ex.isRet = true;
    throw ex;
}

NodeVal Evaluator::performOperUnary(CodeLoc codeLoc, NodeVal oper, Oper op) {
    if (!checkIsEvalVal(oper, true)) return NodeVal();

    EvalVal evalVal = EvalVal::copyNoRef(oper.getEvalVal(), LifetimeInfo());
    TypeTable::Id ty = evalVal.getType();
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
            if (assignBasedOnTypeI(evalVal, -x, ty)) success = true;
        } else if (EvalVal::isF(evalVal, typeTable)) {
            double x = EvalVal::getValueF(evalVal, typeTable).value();
            if (assignBasedOnTypeF(evalVal, -x, ty)) success = true;
        }
    } else if (op == Oper::BIT_NOT) {
        if (EvalVal::isI(evalVal, typeTable)) {
            int64_t x = EvalVal::getValueI(evalVal, typeTable).value();
            if (assignBasedOnTypeI(evalVal, ~x, ty)) success = true;
        } else if (EvalVal::isU(evalVal, typeTable)) {
            uint64_t x = EvalVal::getValueU(evalVal, typeTable).value();
            if (assignBasedOnTypeU(evalVal, ~x, ty)) success = true;
        }
    } else if (op == Oper::NOT) {
        if (EvalVal::isB(evalVal, typeTable)) {
            evalVal.b() = !evalVal.b();
            success = true;
        }
    } else if (op == Oper::BIT_AND) {
        if (oper.hasRef()) {
            evalVal = EvalVal::makeVal(typeTable->addTypeAddrOf(ty), typeTable);
            evalVal.p() = oper.getEvalVal().getRef();
            success = true;
        } else {
            msgs->errorExprAddrOfNonRef(codeLoc);
            errorGiven = true;
        }
    }

    if (!success) {
        if (!errorGiven) msgs->errorExprBadOps(codeLoc, op, true, oper.getType().value(), true);
        return NodeVal();
    }

    return NodeVal(codeLoc, move(evalVal));
}

NodeVal Evaluator::performOperUnaryDeref(CodeLoc codeLoc, const NodeVal &oper, TypeTable::Id resTy) {
    if (!checkIsEvalVal(oper, true)) return NodeVal();

    if (EvalVal::isNull(oper.getEvalVal(), typeTable)) {
        msgs->errorExprDerefNull(codeLoc);
        return NodeVal();
    }

    NodeVal nodeEvalVal = NodeVal::copyNoRef(codeLoc, EvalVal::getPointee(oper.getEvalVal(), symbolTable));
    nodeEvalVal.getEvalVal().getType() = resTy;
    nodeEvalVal.getEvalVal().getRef() = oper.getEvalVal().p();
    return nodeEvalVal;
}

ComparisonSignal Evaluator::performOperComparisonSetUp(CodeLoc codeLoc, std::size_t opersCnt) {
    return ComparisonSignal();
}

optional<bool> Evaluator::performOperComparison(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op, ComparisonSignal &signal) {
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
    bool isTypeCallF = typeTable->worksAsCallable(ty, true);
    bool isTypeCallM = typeTable->worksAsCallable(ty, false);

    optional<int64_t> il, ir;
    optional<uint64_t> ul, ur;
    optional<char> cl, cr;
    optional<double> fl, fr;
    optional<StringPool::Id> strl, strr;
    optional<EvalVal::Pointer> pl, pr;
    optional<bool> bl, br;
    optional<NamePool::Id> idl, idr;
    optional<TypeTable::Id> tyl, tyr;
    optional<optional<FuncId>> callfl, callfr;
    optional<optional<MacroId>> callml, callmr;
    if (isTypeI) {
        il = EvalVal::getValueI(lhs.getEvalVal(), typeTable).value();
        ir = EvalVal::getValueI(rhs.getEvalVal(), typeTable).value();
    } else if (isTypeU) {
        ul = EvalVal::getValueU(lhs.getEvalVal(), typeTable).value();
        ur = EvalVal::getValueU(rhs.getEvalVal(), typeTable).value();
    } else if (isTypeC) {
        cl = lhs.getEvalVal().c8();
        cr = rhs.getEvalVal().c8();
    } else if (isTypeF) {
        fl = EvalVal::getValueF(lhs.getEvalVal(), typeTable).value();
        fr = EvalVal::getValueF(rhs.getEvalVal(), typeTable).value();
    } else if (isTypeStr) {
        strl = lhs.getEvalVal().str().value();
        strr = rhs.getEvalVal().str().value();
    } else if (isTypeAnyP) {
        if (isTypeP) {
            pl = lhs.getEvalVal().p();
            pr = rhs.getEvalVal().p();
        } else {
            pl = pr = nullptr;
        }
    } else if (isTypeB) {
        bl = lhs.getEvalVal().b();
        br = rhs.getEvalVal().b();
    } else if (isTypeId) {
        idl = lhs.getEvalVal().id();
        idr = rhs.getEvalVal().id();
    } else if (isTypeTy) {
        tyl = lhs.getEvalVal().ty();
        tyr = rhs.getEvalVal().ty();
    } else if (isTypeCallF) {
        callfl = lhs.getEvalVal().f();
        callfr = rhs.getEvalVal().f();
    } else if (isTypeCallM) {
        callml = lhs.getEvalVal().m();
        callmr = rhs.getEvalVal().m();
    }

    switch (op) {
    case Oper::EQ:
        if (isTypeI) {
            signal.result = il.value()==ir.value();
            return !signal.result;
        } else if (isTypeU) {
            signal.result = ul.value()==ur.value();
            return !signal.result;
        } else if (isTypeC) {
            signal.result = cl.value()==cr.value();
            return !signal.result;
        } else if (isTypeF) {
            signal.result = fl.value()==fr.value();
            return !signal.result;
        } else if (isTypeStr) {
            signal.result = strl.value()==strr.value();
            return !signal.result;
        } else if (isTypeAnyP) {
            signal.result = pl==pr;
            return !signal.result;
        } else if (isTypeB) {
            signal.result = bl.value()==br.value();
            return !signal.result;
        } else if (isTypeId) {
            signal.result = idl.value()==idr.value();
            return !signal.result;
        } else if (isTypeTy) {
            signal.result = tyl.value()==tyr.value();
            return !signal.result;
        } else if (isTypeCallF) {
            signal.result = callfl.value()==callfr.value();
            return !signal.result;
        } else if (isTypeCallM) {
            signal.result = callml.value()==callmr.value();
            return !signal.result;
        }
        break;
    case Oper::NE:
        if (isTypeI) {
            signal.result = il.value()!=ir.value();
            return !signal.result;
        } else if (isTypeU) {
            signal.result = ul.value()!=ur.value();
            return !signal.result;
        } else if (isTypeC) {
            signal.result = cl.value()!=cr.value();
            return !signal.result;
        } else if (isTypeF) {
            signal.result = fl.value()!=fr.value();
            return !signal.result;
        } else if (isTypeStr) {
            signal.result = strl.value()!=strr.value();
            return !signal.result;
        } else if (isTypeAnyP) {
            signal.result = pl!=pr;
            return !signal.result;
        } else if (isTypeB) {
            signal.result = bl.value()!=br.value();
            return !signal.result;
        } else if (isTypeId) {
            signal.result = idl.value()!=idr.value();
            return !signal.result;
        } else if (isTypeTy) {
            signal.result = tyl.value()!=tyr.value();
            return !signal.result;
        } else if (isTypeCallF) {
            signal.result = callfl.value()!=callfr.value();
            return !signal.result;
        } else if (isTypeCallM) {
            signal.result = callml.value()!=callmr.value();
            return !signal.result;
        }
        break;
    case Oper::LT:
        if (isTypeI) {
            signal.result = il.value()<ir.value();
            return !signal.result;
        } else if (isTypeU) {
            signal.result = ul.value()<ur.value();
            return !signal.result;
        } else if (isTypeC) {
            signal.result = cl.value()<cr.value();
            return !signal.result;
        } else if (isTypeF) {
            signal.result = fl.value()<fr.value();
            return !signal.result;
        }
        break;
    case Oper::LE:
        if (isTypeI) {
            signal.result = il.value()<=ir.value();
            return !signal.result;
        } else if (isTypeU) {
            signal.result = ul.value()<=ur.value();
            return !signal.result;
        } else if (isTypeC) {
            signal.result = cl.value()<=cr.value();
            return !signal.result;
        } else if (isTypeF) {
            signal.result = fl.value()<=fr.value();
            return !signal.result;
        }
        break;
    case Oper::GT:
        if (isTypeI) {
            signal.result = il.value()>ir.value();
            return !signal.result;
        } else if (isTypeU) {
            signal.result = ul.value()>ur.value();
            return !signal.result;
        } else if (isTypeC) {
            signal.result = cl.value()>cr.value();
            return !signal.result;
        } else if (isTypeF) {
            signal.result = fl.value()>fr.value();
            return !signal.result;
        }
        break;
    case Oper::GE:
        if (isTypeI) {
            signal.result = il.value()>=ir.value();
            return !signal.result;
        } else if (isTypeU) {
            signal.result = ul.value()>=ur.value();
            return !signal.result;
        } else if (isTypeC) {
            signal.result = cl.value()>=cr.value();
            return !signal.result;
        } else if (isTypeF) {
            signal.result = fl.value()>=fr.value();
            return !signal.result;
        }
        break;
    default:
        break;
    }

    msgs->errorExprBadOps(rhs.getCodeLoc(), op, false, lhs.getType().value(), true);
    return nullopt;
}

// TODO fix - if processing an operand threw ExceptionEvaluatorJump, teardown won't get called
NodeVal Evaluator::performOperComparisonTearDown(CodeLoc codeLoc, bool success, ComparisonSignal signal) {
    if (!success) return NodeVal();

    EvalVal evalVal = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_BOOL), typeTable);
    evalVal.b() = signal.result;
    return NodeVal(codeLoc, move(evalVal));
}

NodeVal Evaluator::performOperAssignment(CodeLoc codeLoc, const NodeVal &lhs, NodeVal rhs) {
    if (!checkIsEvalVal(lhs, true) || !checkIsEvalVal(rhs, true)) return NodeVal();

    LifetimeInfo lhsLifetimeInfo = lhs.getEvalVal().getLifetimeInfo();

    NodeVal &lhsRefee = EvalVal::getRefee(lhs.getEvalVal(), symbolTable);
    lhsRefee = NodeVal::copyNoRef(lhsRefee.getCodeLoc(), rhs, lhsLifetimeInfo);

    NodeVal nodeVal = NodeVal::moveNoRef(lhs.getCodeLoc(), move(rhs), lhsLifetimeInfo);
    nodeVal.getEvalVal().getRef() = lhs.getEvalVal().getRef();
    return nodeVal;
}

NodeVal Evaluator::performOperIndexArr(CodeLoc codeLoc, NodeVal &base, const NodeVal &ind, TypeTable::Id resTy) {
    if (!checkIsEvalVal(base, true) || !checkIsEvalVal(ind, true)) return NodeVal();

    optional<size_t> index = EvalVal::getValueNonNeg(ind.getEvalVal(), typeTable);
    if (!index.has_value()) {
        msgs->errorInternal(codeLoc);
        return NodeVal();
    }

    if (typeTable->worksAsTypeArr(base.getType().value())) {
        NodeVal nodeVal = NodeVal::copyNoRef(codeLoc, base.getEvalVal().elems()[index.value()], base.getEvalVal().getLifetimeInfo());
        nodeVal.getEvalVal().getType() = resTy;
        if (base.hasRef()) {
            NodeVal &baseRefee = EvalVal::getRefee(base.getEvalVal(), symbolTable);
            nodeVal.getEvalVal().getRef() = &baseRefee.getEvalVal().elems()[index.value()];
        }
        return nodeVal;
    } else if (typeTable->worksAsTypeStr(base.getType().value())) {
        if (!base.getEvalVal().str().has_value()) {
            msgs->errorExprIndexNull(codeLoc);
            return NodeVal();
        }
        StringPool::Id strId = base.getEvalVal().str().value();
        const string &str = stringPool->get(strId);
        if (index.value() > str.size()) {
            msgs->errorExprIndexOutOfBounds(ind.getCodeLoc(), index.value(), str.size()+1);
            return NodeVal();
        }

        // not a ref-val
        EvalVal evalVal = EvalVal::makeVal(resTy, typeTable);
        evalVal.c8() = str[index.value()];
        return NodeVal(codeLoc, move(evalVal));
    } else if (typeTable->worksAsTypeArrP(base.getType().value())) {
        msgs->errorExprIndexNull(codeLoc);
        return NodeVal();
    } else {
        msgs->errorInternal(codeLoc);
        return NodeVal();
    }
}

NodeVal Evaluator::performOperIndex(CodeLoc codeLoc, NodeVal &base, std::uint64_t ind, TypeTable::Id resTy) {
    if (!checkIsEvalVal(base, true)) return NodeVal();

    if (NodeVal::isRawVal(base, typeTable)) {
        NodeVal nodeVal = base.getChild(ind);
        if (NodeVal::isRawVal(nodeVal, typeTable)) {
            nodeVal.getEvalVal().getType() = resTy;
            if (base.hasRef()) {
                NodeVal &baseRefee = EvalVal::getRefee(base.getEvalVal(), symbolTable);
                nodeVal.getEvalVal().getRef() = &baseRefee.getEvalVal().elems()[ind];
            } else {
                nodeVal.getEvalVal().getRef() = nullptr;
            }
        }
        if (base.isInvokeArg() && nodeVal.getLifetimeInfo().has_value()) {
            LifetimeInfo lifetimeInfo = nodeVal.getLifetimeInfo().value();
            lifetimeInfo.invokeArg = true;
            nodeVal.setLifetimeInfo(lifetimeInfo);
        }
        return nodeVal;
    } else {
        NodeVal nodeVal = NodeVal::copyNoRef(codeLoc, base.getEvalVal().elems()[ind], base.getEvalVal().getLifetimeInfo());
        nodeVal.getEvalVal().getType() = resTy;
        if (base.hasRef()) {
            NodeVal &baseRefee = EvalVal::getRefee(base.getEvalVal(), symbolTable);
            nodeVal.getEvalVal().getRef() = &baseRefee.getEvalVal().elems()[ind];
        }
        return nodeVal;
    }
}

// TODO warn on over/underflow; are results correct in these cases?
NodeVal Evaluator::performOperRegular(CodeLoc codeLoc, const NodeVal &lhs, const NodeVal &rhs, Oper op, OperRegAttrs attrs) {
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
        idl = lhs.getEvalVal().id();
        idr = rhs.getEvalVal().id();
    }

    switch (op) {
    case Oper::ADD:
        if (isTypeI) {
            if (assignBasedOnTypeI(evalVal, addWithWrap(il.value(), ir.value()), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(evalVal, ul.value()+ur.value(), ty)) success = true;
        } else if (isTypeF) {
            if (assignBasedOnTypeF(evalVal, fl.value()+fr.value(), ty)) success = true;
        } else if (isTypeId) {
            evalVal.id() = makeIdConcat(lhs.getEvalVal().id(), rhs.getEvalVal().id(), attrs.bare);
            success = true;
        } else if (isTypeRaw) {
            evalVal.elems() = makeRawConcat(lhs.getEvalVal(), rhs.getEvalVal());
            success = true;
        }
        break;
    case Oper::SUB:
        if (isTypeI) {
            if (assignBasedOnTypeI(evalVal, subWithWrap(il.value(), ir.value()), ty)) success = true;
        } else if (isTypeU) {
            if (assignBasedOnTypeU(evalVal, ul.value()-ur.value(), ty)) success = true;
        } else if (isTypeF) {
            if (assignBasedOnTypeF(evalVal, fl.value()-fr.value(), ty)) success = true;
        }
        break;
    case Oper::MUL:
        if (isTypeI) {
            if (assignBasedOnTypeI(evalVal, mulWithWrap(il.value(), ir.value()), ty)) success = true;
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
                evalVal.f32() = fmod(lhs.getEvalVal().f32(), rhs.getEvalVal().f32());
            } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_F64)) {
                evalVal.f64() = fmod(lhs.getEvalVal().f64(), rhs.getEvalVal().f64());
            }
            success = true;
        }
        break;
    case Oper::SHL:
        if (isTypeI) {
            if (assignBasedOnTypeI(evalVal, shlWithWrap(il.value(), ir.value()), ty)) success = true;
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
    default:
        break;
    }

    if (!success) {
        if (!errorGiven) msgs->errorExprBadOps(rhs.getCodeLoc(), op, false, lhs.getType().value(), true);
        return NodeVal();
    }

    return NodeVal(codeLoc, move(evalVal));
}

optional<uint64_t> Evaluator::performSizeOf(CodeLoc codeLoc, TypeTable::Id ty) {
    msgs->errorInternal(codeLoc);
    return nullopt;
}

NodeVal Evaluator::doBlockTearDown(CodeLoc codeLoc, SymbolTable::Block block, bool success, bool jumpingOut) {
    if (!success) return NodeVal();

    if (!jumpingOut && block.type.has_value()) {
        if (!retVal.has_value()) {
            msgs->errorBlockNoPass(codeLoc);
            return NodeVal();
        }

        NodeVal ret = move(retVal.value());
        retVal.reset();
        return move(ret);
    }

    return NodeVal(codeLoc);
}

optional<NodeVal> Evaluator::makeCast(CodeLoc codeLoc, const NodeVal &srcVal, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId) {
    // TODO early catch case when just changing constness
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
        if (!assignBasedOnTypeI(dstEvalVal, (int64_t) srcEvalVal.c8(), dstTypeId) &&
            !assignBasedOnTypeU(dstEvalVal, (uint64_t) srcEvalVal.c8(), dstTypeId) &&
            !assignBasedOnTypeC(dstEvalVal, srcEvalVal.c8(), dstTypeId) &&
            !assignBasedOnTypeB(dstEvalVal, (bool) srcEvalVal.c8(), dstTypeId)) {
            return nullopt;
        }
    } else if (typeTable->worksAsTypeB(srcTypeId)) {
        if (!assignBasedOnTypeI(dstEvalVal, srcEvalVal.b() ? 1 : 0, dstTypeId) &&
            !assignBasedOnTypeU(dstEvalVal, srcEvalVal.b() ? 1 : 0, dstTypeId) &&
            !assignBasedOnTypeB(dstEvalVal, srcEvalVal.b(), dstTypeId) &&
            !assignBasedOnTypeId(dstEvalVal, srcEvalVal.b(), dstTypeId)) {
            return nullopt;
        }
    } else if (typeTable->worksAsTypeStr(srcTypeId)) {
        if (srcEvalVal.str().has_value()) {
            const string &str = stringPool->get(srcEvalVal.str().value());
            if (typeTable->worksAsTypeStr(dstTypeId)) {
                dstEvalVal.str() = srcEvalVal.str();
            } else if (typeTable->worksAsTypeB(dstTypeId)) {
                dstEvalVal.b() = true;
            } else if (typeTable->worksAsTypeCharArrOfLen(dstTypeId, LiteralVal::getStringLen(str))) {
                optional<EvalVal> arrEvalVal = makeArray(dstTypeId);
                if (!arrEvalVal.has_value()) { 
                    return nullopt;
                } else {
                    dstEvalVal = arrEvalVal.value();
                    for (size_t i = 0; i < LiteralVal::getStringLen(str); ++i) {
                        dstEvalVal.elems()[i].getEvalVal().c8() = str[i];
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
                !typeTable->worksAsTypeAnyP(dstTypeId) &&
                !typeTable->worksAsCallable(dstTypeId)) {
                return nullopt;
            }
        } else {
            if (typeTable->isImplicitCastable(typeTable->extractExplicitTypeBaseType(srcTypeId), typeTable->extractExplicitTypeBaseType(dstTypeId))) {
                if (!assignBasedOnTypeP(dstEvalVal, srcEvalVal.p(), dstTypeId)) {
                    return nullopt;
                }
            } else {
                return nullopt;
            }
        }
    } else if (typeTable->worksAsPrimitive(srcTypeId, TypeTable::P_TYPE)) {
        if (typeTable->worksAsPrimitive(dstTypeId, TypeTable::P_TYPE)) {
            dstEvalVal = EvalVal::copyNoRef(srcEvalVal, LifetimeInfo());
            dstEvalVal.getType() = dstTypeId;
        } else if (typeTable->worksAsPrimitive(dstTypeId, TypeTable::P_ID)) {
            optional<NamePool::Id> id = makeIdFromTy(srcEvalVal.ty());
            if (id.has_value()) dstEvalVal.id() = id.value();
            else return nullopt;
        } else {
            return nullopt;
        }
    } else if (typeTable->worksAsTuple(srcTypeId)) {
        if (typeTable->worksAsTuple(dstTypeId)) {
            const TypeTable::Tuple &tupSrc = *typeTable->extractTuple(srcTypeId);
            const TypeTable::Tuple &tupDst = *typeTable->extractTuple(dstTypeId);

            if (tupSrc.elements.size() != tupDst.elements.size()) return nullopt;

            for (size_t i = 0; i < tupSrc.elements.size(); ++i) {
                optional<NodeVal> elemCast = makeCast(codeLoc, srcEvalVal.elems()[i], tupSrc.elements[i], tupDst.elements[i]);
                if (!elemCast.has_value()) return nullopt;

                dstEvalVal.elems()[i] = move(elemCast.value());
            }
        } else {
            return nullopt;
        }
    } else if (typeTable->worksAsCallable(srcTypeId)) {
        if (typeTable->worksAsTypeAnyP(dstTypeId)) {
            if (!EvalVal::isCallableNoValue(srcEvalVal, typeTable) ||
                (!assignBasedOnTypeP(dstEvalVal, nullptr, dstTypeId) && !typeTable->worksAsTypeAnyP(dstTypeId)))
                return nullopt;
        } else if (typeTable->worksAsTypeB(dstTypeId)) {
            if (!assignBasedOnTypeB(dstEvalVal, !EvalVal::isCallableNoValue(srcEvalVal, typeTable), dstTypeId))
                return nullopt;
        } else if (typeTable->isImplicitCastable(typeTable->extractExplicitTypeBaseType(srcTypeId), typeTable->extractExplicitTypeBaseType(dstTypeId))) {
            dstEvalVal = EvalVal::copyNoRef(srcEvalVal, LifetimeInfo());
            dstEvalVal.getType() = dstTypeId;
        } else {
            return nullopt;
        }
    } else {
        // other types are only castable when changing constness
        if (typeTable->isImplicitCastable(typeTable->extractExplicitTypeBaseType(srcTypeId), typeTable->extractExplicitTypeBaseType(dstTypeId))) {
            // no action is needed in case of a cast
            dstEvalVal = EvalVal::copyNoRef(srcEvalVal, LifetimeInfo());
            dstEvalVal.getType() = dstTypeId;
        } else {
            return nullopt;
        }
    }

    dstEvalVal.getLifetimeInfo() = LifetimeInfo();
    dstEvalVal.getLifetimeInfo().noDrop = srcEvalVal.getLifetimeInfo().noDrop;

    return NodeVal(codeLoc, move(dstEvalVal));
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
    optional<string> str = typeTable->makeBinString(x, namePool, true);
    if (!str.has_value()) return nullopt;

    stringstream ss;
    ss << "$T" << str.value();
    return namePool->add(ss.str());
}

NamePool::Id Evaluator::makeIdConcat(NamePool::Id lhs, NamePool::Id rhs, bool bare) {
    stringstream ss;
    ss << namePool->get(lhs) << (bare ? "" : "$+") << namePool->get(rhs);
    return namePool->add(ss.str());
}

vector<NodeVal> Evaluator::makeRawConcat(const EvalVal &lhs, const EvalVal &rhs) const {
    vector<NodeVal> elems;
    elems.reserve(lhs.elems().size()+rhs.elems().size());

    for (const auto &it : lhs.elems()) {
        elems.push_back(it);
    }
    for (const auto &it : rhs.elems()) {
        elems.push_back(it);
    }

    return elems;
}

bool Evaluator::assignBasedOnTypeI(EvalVal &val, int64_t x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_I8)) {
        val.i8() = (int8_t) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_I16)) {
        val.i16() = (int16_t) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_I32)) {
        val.i32() = (int32_t) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_I64)) {
        val.i64() = (int64_t) x;
    } else {
        return false;
    }

    return true;
}

bool Evaluator::assignBasedOnTypeU(EvalVal &val, uint64_t x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_U8)) {
        val.u8() = (uint8_t) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_U16)) {
        val.u16() = (uint16_t) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_U32)) {
        val.u32() = (uint32_t) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_U64)) {
        val.u64() = (uint64_t) x;
    } else {
        return false;
    }

    return true;
}

bool Evaluator::assignBasedOnTypeF(EvalVal &val, double x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_F32)) {
        val.f32() = (float) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_F64)) {
        val.f64() = (double) x;
    } else {
        return false;
    }

    return true;
}

bool Evaluator::assignBasedOnTypeC(EvalVal &val, char x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_C8)) {
        val.c8() = x;
    } else {
        return false;
    }

    return true;
}

bool Evaluator::assignBasedOnTypeB(EvalVal &val, bool x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_BOOL)) {
        val.b() = x;
    } else {
        return false;
    }

    return true;
}

bool Evaluator::assignBasedOnTypeP(EvalVal &val, EvalVal::Pointer x, TypeTable::Id ty) {
    if (typeTable->worksAsTypeP(ty)) {
        val.p() = x;
    } else {
        return false;
    }

    return true;
}

bool Evaluator::assignBasedOnTypeId(EvalVal &val, std::uint64_t x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_ID)) {
        val.id() = makeIdFromU(x);
    } else {
        return false;
    }

    return true;
}

bool Evaluator::assignBasedOnTypeId(EvalVal &val, bool x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_ID)) {
        val.id() = makeIdFromB(x);
    } else {
        return false;
    }

    return true;
}