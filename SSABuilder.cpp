#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "SSABuilder.hpp"
#include "RegAlloc.hpp"
#include "ralloc.h"
#include "List.hpp"
#include "ScriptUtils.h"
#include "Builtins.hpp"

void BasicBlock::addPred(BasicBlock *newPred)
{
    preds.gotoLast();
    preds.insertAfter(newPred);
}

// returns true if last instruction in block is an unconditional jump (JMP/RETURN)
bool BasicBlock::endsWithJump()
{
    Instruction *lastInstruction = end->getPrevious()->value;
    return (lastInstruction->isJump() || lastInstruction->op == OP_RETURN);
}

void BasicBlock::printName()
{
    printf("BB:%i", id);
}

void BasicBlock::print()
{
    printName();
    printf(" (");
    foreach_list(preds, BasicBlock*, iter)
    {
        printf("%i", iter.value()->id);
        if (iter.hasNext()) printf(", ");
    }
    printf(")");
    
    if (loop)
    {
        printf(" (loop %i)", loop->header->id);
    }
}

bool BasicBlock::dominates(BasicBlock *other, BasicBlock *without, int numBlocks)
{
    BitSet tested(numBlocks, true);
    return this->dominates(other, without, &tested);
}

bool BasicBlock::dominates(BasicBlock *b, BasicBlock *without, BitSet *tested)
{
    if (tested->test(b->id)) return true;
    tested->set(b->id);
    if (b == this) return true;
    if (b == without) return false;
    if (b->preds.isEmpty()) return false; // start block
    foreach_list(b->preds, BasicBlock*, iter)
    {
        if (!this->dominates(iter.value(), without, tested)) return false;
    }
    return true;
}

const char *SSABuilder::getIdentString(const char *variable, BasicBlock *block)
{
    snprintf(identBuf, sizeof(identBuf), "%s:%i", variable, block->id);
    return identBuf;
}

void SSABuilder::writeVariable(const char *variable, BasicBlock *block, RValue *value)
{
    // currentDef[variable][block] ← value
    const char *ident = getIdentString(variable, block);
    if (currentDef.findByName(ident))
    {
        currentDef.update(value);
    }
    else
    {
        currentDef.gotoLast();
        currentDef.insertAfter(value, ident);
    }
    block->hasAssignment = true;
}

RValue *SSABuilder::readVariable(const char *variable, BasicBlock *block)
{
    const char *ident = getIdentString(variable, block);
    if (currentDef.findByName(ident)) // currentDef[variable] contains block
    {
        // local value numbering
        return currentDef.retrieve();
    }
    // global value numbering
    return readVariableRecursive(variable, block);
}

RValue *SSABuilder::readVariableRecursive(const char *variable, BasicBlock *block)
{
    RValue *val;
    if (!block->isSealed)
    {
        // Incomplete CFG
        Phi *phi = new(memCtx) Phi(valueId());
        insertInstructionAtStart(phi, block);
        // incompletePhis[block][variable] ← val
        if (block->incompletePhis.findByName(variable))
        {
            block->incompletePhis.update(phi);
        }
        else
        {
            block->incompletePhis.gotoLast();
            block->incompletePhis.insertAfter(phi, variable);
        }
        val = phi->value();
    }
    else if (block->preds.size() == 1)
    {
        // Optimize the common case of one predecessor: No phi needed
        val = readVariable(variable, block->preds.retrieve());
    }
    else
    {
        // Break potential cycles with operandless phi
        Phi *phi = new(memCtx) Phi(valueId());
        insertInstructionAtStart(phi, block);
        writeVariable(variable, block, phi->value());
        val = addPhiOperands(variable, block, phi);
    }
    writeVariable(variable, block, val);
    return val;
}

RValue *SSABuilder::addPhiOperands(const char *variable, BasicBlock *block, Phi *phi)
{
    // Determine operands from predecessors
    phi->sourceBlocks = ralloc_array(memCtx, BasicBlock*, block->preds.size());
    int i = 0;
    foreach_list(block->preds, BasicBlock*, iter)
    {
        BasicBlock *pred = iter.value();
        RValue *operand = readVariable(variable, pred);
        phi->appendOperand(operand);
        phi->sourceBlocks[i++] = pred;
    }
    return tryRemoveTrivialPhi(phi);
}

RValue *SSABuilder::tryRemoveTrivialPhi(Phi *phi)
{
    // printf("tryRemoveTrivialPhi: "); phi->print();
    RValue *same = NULL;
    foreach_list(phi->operands, RValue*, iter)
    {
        RValue *op = iter.value();
        if (op == same || op == phi->value())
            continue; // Unique value or self−reference
        if (same != NULL) {
            // printf("not trivial: "); same->printDst(); printf(" and "); op->printDst(); printf("\n");
            return phi->value(); // The phi merges at least two values: not trivial
        }
        same = op;
    }
    if (same == NULL)
    {
        same = new(memCtx) Undef(); // The phi is unreachable or in the start block
    }
    // printf("trivial phi; replace with "); same->printDst(); printf("\n");
    // remember all users except the phi itself
    List<Phi*> phiUsers;
    foreach_list(phi->value()->users, Instruction*, iter)
    {
        Instruction *use = iter.value();
        if (use->isPhi() && use != phi)
        {
            phiUsers.insertAfter(use->asPhi(), NULL);
        }
    }
    phi->value()->replaceBy(same); // Reroute all uses of phi to same and remove phi
    foreach_list(currentDef, RValue*, iter)
    {
        // replace variable value with same
        if (iter.value() == phi->value())
            iter.update(same);
    }
    
    // Try to recursively remove all phi users, which might have become trivial
    foreach_list(phiUsers, Phi*, iter)
    {
        tryRemoveTrivialPhi(iter.value());
    }
    return same;
}

void SSABuilder::sealBlock(BasicBlock *block)
{
    foreach_list(block->incompletePhis, Phi*, iter)
    {
        addPhiOperands(iter.name(), block, iter.value());
    }
    block->isSealed = true; // sealedBlocks.add(block)
}

// create BB and insert after the given block
BasicBlock *SSABuilder::createBBAfter(BasicBlock *existingBB)
{
    BasicBlock *newBB = new(memCtx) BasicBlock(nextBBId++);
    basicBlockList.gotoLast();
    basicBlockList.insertAfter(newBB, NULL);
    if (existingBB) instructionList.setCurrent(existingBB->end);

    // insert start/end instructions into instruction list
    Instruction *start = new(memCtx) BlockDecl(newBB), *end = new(memCtx) NoOp();
    instructionList.insertAfter(start, NULL);
    newBB->start = instructionList.currentNode();
    instructionList.insertAfter(end, NULL);
    newBB->end = instructionList.currentNode();
    return newBB;
}

// insert instruction at end of basic block
void SSABuilder::insertInstruction(Instruction *inst, BasicBlock *bb)
{
    instructionList.setCurrent(bb->end);
    instructionList.insertBefore(inst, NULL);
    inst->block = bb;
}

// insert instruction at start of basic block
void SSABuilder::insertInstructionAtStart(Instruction *inst, BasicBlock *bb)
{
    instructionList.setCurrent(bb->start);
    instructionList.insertAfter(inst, NULL);
    inst->block = bb;
}

// insert instruction at end of basic block
Constant *SSABuilder::addConstant(ScriptVariant sv)
{
    // TODO: prevent duplicates
    constantList.gotoLast();
    Constant *c = new(memCtx) Constant(sv);
    constantList.insertAfter(c, NULL);
    return c;
}

void SSABuilder::printInstructionList()
{
    foreach_list(instructionList, Instruction*, iter)
    {
        iter.value()->print();
    }
}

// dead code elimination pass
void SSABuilder::removeDeadCode()
{
    instructionList.gotoFirst();
    foreach_list(instructionList, Instruction *, iter)
    {
        Instruction *inst = iter.value();
        if (inst->isExpression())
        {
            Expression *expr = inst->asExpression();
            if (expr->isDead())
            {
                // RValue *undefined = new(memCtx) Undef();
                foreach_list(expr->operands, RValue*, iter)
                {
                    iter.value()->unref(expr);
                }
                iter.remove();
                continue;
            }
        }
    }
}

void SSABuilder::prepareForRegAlloc()
{
    // insert phi moves
    foreach_list(basicBlockList, BasicBlock*, iter)
    {
        BasicBlock *block = iter.value();
        Node<Instruction*> *instNode = block->start;
        while ((instNode = instNode->getNext()))
        {
            Instruction *inst = instNode->value;
            if (inst->op != OP_PHI) break; // only process phis, which are always at start of block
            Phi *phi = (Phi*) inst;
            // phi->print();
            // insert moves for phi before last jump in src block
            int i = 0;
            foreach_list(phi->operands, RValue*, srcIter)
            {
                RValue *phiSrc = srcIter.value();
                BasicBlock *srcBlock = phi->sourceBlocks[i++];
                Node<Instruction*> *insertPoint = srcBlock->end->getPrevious();
                Expression *move = new(memCtx) Expression(OP_MOV, valueId(), phiSrc);
                move->block = srcBlock;
                while (insertPoint->value->isJump())
                    insertPoint = insertPoint->getPrevious();
                instructionList.setCurrent(insertPoint);
                instructionList.insertAfter(move, NULL);
                // replace reference
                srcIter.update(move->value());
                move->value()->ref(phi);
                phiSrc->unref(phi);
                move->isPhiMove = true;

                // replace other references if we can, to improve register allocation
                int numBBs = basicBlockList.size();
                if (!phiSrc->isTemporary()) continue; // no advantage to this if src isn't a temp
                foreach_list(phiSrc->users, Instruction*, refIter)
                {
                    Instruction *inst2 = refIter.value();
                    if (inst2->isPhi() || inst2->isPhiMove) continue;
                    // If user is a jump in this block, it's at the end so the phi move
                    // dominates it. We can also replace if the phi move dominates the
                    // user without passing through the phi.
                    if ((inst2->block == srcBlock && inst2->isJump()) ||
                        (inst2->block != srcBlock && srcBlock->dominates(inst2->block, block, numBBs)))
                    {
                        // replace the reference
                        foreach_list(inst2->operands, RValue*, srcIter2)
                        {
                            if (srcIter2.value() == phiSrc)
                            {
                                refIter.remove();
                                srcIter2.update(move->value());
                                move->value()->ref(inst2);
                            }
                        }
                    }
                }
            }
        }
        
        // build successor lists
        foreach_list(block->preds, BasicBlock*, predIter)
        {
            predIter.value()->succs.insertAfter(block, NULL);
        }
    }
    
    // build temporary list and assign sequential indices to instructions
    assert(temporaries.isEmpty());
    int tempCount = 0, instCount = 0;
    foreach_list(instructionList, Instruction*, iter)
    {
        iter.value()->seqIndex = instCount++;
        if (!iter.value()->isExpression()) continue;
        Expression *expr = iter.value()->asExpression();
        if (!expr->value()->isTemporary()) continue;
        Temporary *temp = expr->value()->asTemporary();
        temp->id = tempCount++; // renumber temporaries
        temporaries.insertAfter(temp);
    }

    // build basic block live sets
    foreach_list(basicBlockList, BasicBlock*, iter)
    {
        BasicBlock *block = iter.value();
        block->liveIn.allocate(tempCount, true);
        block->liveOut.allocate(tempCount, true);
        block->phiDefs.allocate(tempCount, true);
        block->phiUses.allocate(tempCount, true);
        block->processed = false;
    }

    // build phiUses and phiDefs sets
    foreach_list(basicBlockList, BasicBlock*, iter)
    {
        BasicBlock *block = iter.value();
        Node<Instruction*> *instNode = block->start;
        while ((instNode = instNode->getNext()))
        {
            Instruction *inst = instNode->value;
            if (inst->op != OP_PHI) break; // only process phis, which are always at start of block
            Phi *phi = (Phi*) inst;
            // update phiDefs with phi definition
            block->phiDefs.set(phi->value()->asTemporary()->id);
            foreach_list(phi->operands, RValue*, srcIter)
            {
                // update phiUses for each phi operand
                Temporary *phiSrc = srcIter.value()->asTemporary();
                phiSrc->expr->block->phiUses.set(phiSrc->id);
            }
        }
    }
}

// predecessor: the basic block right before the loop
Loop::Loop(BasicBlock *header, Loop *parent)
{
    this->header = header;
    this->parent = parent;
}

void GlobalState::declareGlobalVariable(const char *varName, ScriptVariant initialValue)
{
    assert(!globalVariables.findByName(varName));
    globalVariables.gotoLast();
    globalVariables.insertAfter(initialValue, varName);
    //printf("Declare global variable '%s'\n", varName);
}

// bool GlobalState::initializeGlobalVariable(const char *varName, ScriptVariant *value)
// {
// }

GlobalVarRef *GlobalState::readGlobalVariable(const char *varName, void *memCtx)
{
    if (globalVariables.findByName(varName))
    {
        return new(memCtx) GlobalVarRef(globalVariables.getIndex());
    }
    else
        return NULL;
}

SSABuildUtil::SSABuildUtil(SSABuilder *builder, GlobalState *globalState)
    : builder(builder), globalState(globalState), currentBlock(NULL)
{
    currentLoop = NULL;
}

// declare a parameter to this function
void SSABuildUtil::addParam(const char *name)
{
    Param *param = new(builder->memCtx) Param(builder->paramCount);
    builder->paramCount++;
    declareVariable(name);
    writeVariable(name, param);
}

// used for constant folding
Constant *SSABuildUtil::applyOp(OpCode op, ScriptVariant *src0, ScriptVariant *src1)
{
    ScriptVariant result;
    CCResult success;

    switch (op)
    {
    // unary
    case OP_NEG: success = ScriptVariant_Neg(&result, src0); break;
    case OP_BOOL_NOT: success = ScriptVariant_Boolean_Not(&result, src0); break;
    case OP_BIT_NOT: success = ScriptVariant_Bit_Not(&result, src0); break;
    case OP_INC: success = ScriptVariant_Inc(&result, src0); break;
    case OP_DEC: success = ScriptVariant_Dec(&result, src0); break;
    case OP_BOOL: success = ScriptVariant_ToBoolean(&result, src0); break;
    // binary
    case OP_BIT_OR: success = ScriptVariant_Bit_Or(&result, src0, src1); break;
    case OP_XOR: success = ScriptVariant_Xor(&result, src0, src1); break;
    case OP_BIT_AND: success = ScriptVariant_Bit_And(&result, src0, src1); break;
    case OP_EQ: success = ScriptVariant_Eq(&result, src0, src1); break;
    case OP_NE: success = ScriptVariant_Ne(&result, src0, src1); break;
    case OP_LT: success = ScriptVariant_Lt(&result, src0, src1); break;
    case OP_GT: success = ScriptVariant_Gt(&result, src0, src1); break;
    case OP_GE: success = ScriptVariant_Ge(&result, src0, src1); break;
    case OP_LE: success = ScriptVariant_Le(&result, src0, src1); break;
    case OP_SHL: success = ScriptVariant_Shl(&result, src0, src1); break;
    case OP_SHR: success = ScriptVariant_Shr(&result, src0, src1); break;
    case OP_ADD: success = ScriptVariant_AddFolding(&result, src0, src1); break;
    case OP_SUB: success = ScriptVariant_Sub(&result, src0, src1); break;
    case OP_MUL: success = ScriptVariant_Mul(&result, src0, src1); break;
    case OP_DIV: success = ScriptVariant_Div(&result, src0, src1); break;
    case OP_REM: success = ScriptVariant_Rem(&result, src0, src1); break;
    default: success = CC_FAIL;
    }

    if (success == CC_OK)
    {
        return builder->addConstant(result);
    }
    return NULL;
}

RValue *SSABuildUtil::mkUnaryOp(OpCode op, RValue *src)
{
    RValue *result = NULL;
    if (op == OP_BOOL && src->isBoolValue())
    {
        // no-op if src is guaranteed to be integer 0 or 1
        return src;
    }
    else if (op == OP_BOOL_NOT && src->isTemporary() &&
             src->asTemporary()->expr->op == OP_BOOL)
    {
        // replace bool_not(bool(x)) with bool_not(x) since conversion to
        // a boolean is implicit in the bool_not operation
        src = src->asTemporary()->expr->src(0);
    }
    if (src->isConstant()) // pre-evaluate expression
    {
        result = applyOp(op, &src->asConstant()->constValue, NULL);
    }
    if (result == NULL) // non-constant operands
    {
        Expression *inst = new(builder->memCtx) Expression(op, builder->valueId(), src);
        builder->insertInstruction(inst, currentBlock);
        result = inst->value();
    }
    assert(result != NULL);
    return result;
}

RValue *SSABuildUtil::mkBinaryOp(OpCode op, RValue *src0, RValue *src1)
{
    RValue *result = NULL;
    if (src0->isConstant() && src1->isConstant()) // pre-evaluate expression
    {
        result = applyOp(op,
                         &src0->asConstant()->constValue,
                         &src1->asConstant()->constValue);
    }
    if (result == NULL) // non-constant operands
    {
        Expression *inst = new(builder->memCtx) Expression(op, builder->valueId(), src0, src1);
        builder->insertInstruction(inst, currentBlock);
        result = inst->value();
    }
    assert(result != NULL);
    return result;
}

Constant *SSABuildUtil::mkConstInt(int32_t val)
{
    ScriptVariant var = {{0}, VT_INTEGER};
    var.lVal = val;
    var.vt = VT_INTEGER;
    return builder->addConstant(var);
}

Constant *SSABuildUtil::mkConstString(char *val)
{
    ScriptVariant var;
    ScriptVariant_ParseStringConstant(&var, val);
    return builder->addConstant(var);
}

Constant *SSABuildUtil::mkConstFloat(double val)
{
    ScriptVariant var;
    var.dblVal = val;
    var.vt = VT_DECIMAL;
    return builder->addConstant(var);
}

Constant *SSABuildUtil::mkNull()
{
    ScriptVariant var = {{0}, VT_EMPTY};
    return builder->addConstant(var);
}

Jump *SSABuildUtil::mkJump(OpCode op, BasicBlock *target, RValue *src0, RValue *src1)
{
    // if the condition is the result of an OP_BOOL instruction, we can use
    // the source of that instruction as the condition instead
    if ((op == OP_BRANCH_TRUE || op == OP_BRANCH_FALSE) && src0 && src0->isTemporary())
    {
        Temporary *temp = src0->asTemporary();
        if (temp->expr->op == OP_BOOL)
            src0 = temp->expr->src(0);
    }
    Jump *inst = new(builder->memCtx) Jump(op, target, src0, src1);
    builder->insertInstruction(inst, currentBlock);
    return inst;
}

Instruction *SSABuildUtil::mkReturn(RValue *src0)
{
    Instruction *inst = new(builder->memCtx) Instruction(OP_RETURN);
    if (src0 != NULL)
    {
        inst->appendOperand(src0);
    }
    builder->insertInstruction(inst, currentBlock);
    return inst;
}

RValue *SSABuildUtil::mkMove(RValue *src)
{
    Expression *inst = new(builder->memCtx) Expression(OP_MOV, builder->valueId(), src);
    builder->insertInstruction(inst, currentBlock);
    return inst->value();
}

RValue *SSABuildUtil::mkGetGlobal(RValue *src)
{
    Expression *inst = new(builder->memCtx) Expression(OP_GET_GLOBAL, builder->valueId(), src);
    builder->insertInstruction(inst, currentBlock);
    return inst->value();
}

RValue *SSABuildUtil::mkObject()
{
    Expression *inst = new(builder->memCtx) Expression(OP_MKOBJECT, builder->valueId(), mkConstInt(0));
    builder->insertInstruction(inst, currentBlock);
    return inst->value();
}

RValue *SSABuildUtil::mkList()
{
    Expression *inst = new(builder->memCtx) Expression(OP_MKLIST, builder->valueId(), mkConstInt(0));
    builder->insertInstruction(inst, currentBlock);
    return inst->value();
}

RValue *SSABuildUtil::mkGet(RValue *object, RValue *key)
{
    RValue *value = mkBinaryOp(OP_GET, object, key);
    value->lvalue = new(builder->memCtx) LValue(object, key);
    return value;
}

Instruction *SSABuildUtil::mkSet(RValue *object, RValue *key, RValue *value)
{
    Instruction *inst = new(builder->memCtx) Instruction(OP_SET);
    inst->appendOperand(object);
    inst->appendOperand(key);
    inst->appendOperand(value);
    builder->insertInstruction(inst, currentBlock);
    return inst;
}

Export *SSABuildUtil::mkExport(GlobalVarRef *dst, RValue *src)
{
    Export *inst = new(builder->memCtx) Export(dst, src);
    builder->insertInstruction(inst, currentBlock);
    return inst;
}

// starts a function call but doesn't put it in the instruction list
FunctionCall *SSABuildUtil::startFunctionCall(const char *name)
{
    return new(builder->memCtx) FunctionCall(name, builder->valueId());
}

FunctionCall *SSABuildUtil::startMethodCall(int methodIndex, RValue *target)
{
    FunctionCall *call = new(builder) FunctionCall(getMethodName(methodIndex), builder->valueId());
    call->op = OP_CALL_METHOD;
    call->builtinRef = methodIndex;
    call->appendOperand(target);
    return call;
}

// inserts a finished function call instruction at the end of the current block
void SSABuildUtil::insertFunctionCall(FunctionCall *call)
{
    builder->insertInstruction(call, currentBlock);
}

Undef *SSABuildUtil::undef() // get an undefined value
{
    return new(builder->memCtx) Undef();
}

// returns true on success, false if varName is already defined
bool SSABuildUtil::declareVariable(const char *varName)
{
    // see if there is already a variable defined with this name (error)
    Symbol *existingSymbol;
    if (symbolTable.findSymbol(varName, &existingSymbol))
        return false;

    // variable is not defined, so create symbol and add it to symbol table
    Symbol *sym = (Symbol *) malloc(sizeof(Symbol));
    Symbol_Init(sym, varName, NULL);
    symbolTable.addSymbol(sym);
    return true;
}

// returns true if varName is valid in current scope
bool SSABuildUtil::writeVariable(const char *varName, RValue *value)
{
    Symbol *sym;
    bool found = symbolTable.findSymbol(varName, &sym);
    if (found)
    {
        builder->writeVariable(varName, currentBlock, value);
        return true;
    }
    else
    {
        GlobalVarRef *global = globalState->readGlobalVariable(varName, builder->memCtx);
        if (global)
        {
            mkExport(global, value);
            return true;
        }
        else return false;
    }
}

bool SSABuildUtil::mkAssignment(LValue *lhs, RValue *rhs)
{
    if (lhs->varName)
    {
        return writeVariable(lhs->varName, rhs);
    }
    else
    {
        assert(lhs->parent && lhs->key);
        mkSet(lhs->parent, lhs->key, rhs);
        return true;
    }
}

// returns an Undef if varName is invalid in current scope
RValue *SSABuildUtil::readVariable(const char *varName)
{
    Symbol *sym;
    bool found = symbolTable.findSymbol(varName, &sym);
    if (found)
    {
        RValue *value = builder->readVariable(varName, currentBlock);
        value->lvalue = new(builder->memCtx) LValue(varName);
        return value;
    }
    else
    {
        // it's either a global variable or undefined
        GlobalVarRef *global = globalState->readGlobalVariable(varName, builder->memCtx);
        if (global)
        {
            RValue *value = mkGetGlobal(global);
            value->lvalue = new(builder->memCtx) LValue(varName);
            return value;
        }
        else
        {
            // TODO: fix parser so we don't have to set this
            RValue *value = undef();
            value->lvalue = new(builder->memCtx) LValue(varName);
            return value;
        }
    }
}

void SSABuildUtil::pushScope()
{
    symbolTable.pushScope();
}

void SSABuildUtil::popScope()
{
    symbolTable.popScope();
}

void SSABuildUtil::pushLoop(Loop *loop)
{
    loop->parent = currentLoop;
    if (currentLoop == NULL) // this is a top-level loop
    {
        builder->loops.insertAfter(loop);
    }
    else
    {
        currentLoop->children.insertAfter(loop);
    }
    currentLoop = loop;
}

void SSABuildUtil::popLoop()
{
    assert(currentLoop);
    currentLoop = currentLoop->parent;
}

BasicBlock *SSABuildUtil::createBBAfter(BasicBlock *existingBB, Loop *loop)
{
    BasicBlock *block = builder->createBBAfter(existingBB);
    if (!loop) loop = currentLoop;
    block->loop = loop;
    if (loop) loop->nodes.insertAfter(block);
    return block;
}

