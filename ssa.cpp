#include "ralloc.h"
#include "List.h"
#include <string.h>
#include <assert.h>

#include "ssa.h"

void Instruction::appendOperand(RValue *value)
{
    operands.gotoLast();
    operands.insertAfter(value);
}

RValue *Instruction::src(int index)
{
    operands.gotoFirst();
    for(int i=0; i<index; i++) operands.gotoNext();
    return operands.retrieve();
}

// needs full definition of RValue for ref and unref
void Instruction::setSrc(int index, RValue *val)
{
    if (!operands.gotoIndex(index)) return; // TODO assert
    RValue *oldVal = operands.retrieve();
    if (oldVal)
    {
        oldVal->unref(this);
    }
    operands.update(val);
    if (val)
    {
        val->ref(this);
    }
}

void Instruction::print()
{
    printf("%s ", opCodeNames[op]);
    foreach_list(operands, RValue, iter)
    {
        iter.value()->printDst();
        printf(" ");
    }
    printf("\n");
}

RValue::RValue(OpCode opCode, RValue *src0, RValue *src1)
    : Instruction(opCode)
{
    if (src0)
        appendOperand(src0);
    if (src1)
        appendOperand(src1);
}

void RValue::replaceBy(RValue *newValue)
{
    while (!users.isEmpty())
    {
        Instruction *inst = users.retrieve();
        int numSrc = inst->operands.size();
        for (int i=0; i < numSrc; i++)
        {
            if (inst->src(i) == this)
            {
                int refsBefore = users.size();
                inst->setSrc(i, newValue);
                // setSrc should call this->unref()
                assert(users.size() == refsBefore - 1);
                break;
            }
        }
    }
}

// referenced by another instruction
void RValue::ref(Instruction *inst)
{
    users.gotoLast();
    users.insertAfter(inst, NULL);
}

// unreferenced by other instruction
void RValue::unref(Instruction *inst)
{
    // inst will be in users list more than once if if references
    // this value more than once
    if (users.includes(inst))
    {
        users.remove();
    }
}

bool RValue::isConstant()
{
    return false;
}

void RValue::print()
{
    printf("%s ", opCodeNames[op]);
    printDst();
    printf(" <= ");
    foreach_list(operands, RValue, iter)
    {
        iter.value()->printDst();
        printf(" ");
    }
    printf("\n");
}

void RValue::printDst()
{
    printf("%%val");
}

Constant::Constant(ScriptVariant val)
    : RValue(OP_CONST)
{
    constValue = val;
}

Constant::Constant(LONG intVal)
    : RValue(OP_CONST)
{
    constValue.vt = VT_INTEGER;
    constValue.lVal = intVal;
}

bool Constant::isConstant()
{
    return true;
}

void Constant::print()
{
    printf("%s ", opCodeNames[op]);
    printDst();
    printf(" <= %i\n", constValue.lVal);
}

void BasicBlock::addPred(BasicBlock *newPred)
{
    preds.gotoLast();
    preds.insertAfter(newPred);
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
        val = new(memCtx) Phi();
        insertInstruction(val, block);
        // incompletePhis[block][variable] ← val
        if (block->incompletePhis.findByName(variable))
        {
            block->incompletePhis.update((Phi*)val);
        }
        else
        {
            block->incompletePhis.gotoLast();
            block->incompletePhis.insertAfter((Phi*)val, variable);
        }
    }
    else if (block->preds.size() == 1)
    {
        // Optimize the common case of one predecessor: No phi needed
        val = readVariable(variable, block->preds.retrieve());
    }
    else
    {
        // Break potential cycles with operandless phi
        val = new(memCtx) Phi();
        insertInstruction(val, block);
        writeVariable(variable, block, val);
        val = addPhiOperands(variable, block, (Phi*)val);
    }
    writeVariable(variable, block, val);
    return val;
}

RValue *SSABuilder::addPhiOperands(const char *variable, BasicBlock *block, Phi *phi)
{
    // Determine operands from predecessors
    foreach_list(block->preds, BasicBlock, iter)
    {
        phi->appendOperand(readVariable(variable, iter.value()));
    }
    return tryRemoveTrivialPhi(phi);
}

RValue *SSABuilder::tryRemoveTrivialPhi(Phi *phi)
{
    RValue *same = NULL;
    foreach_list(phi->operands, RValue, iter)
    {
        RValue *op = iter.value();
        if (op == same || op == phi)
            continue; // Unique value or self−reference
        if (same != NULL)
            return phi; // The phi merges at least two values: not trivial
        same = op;
    }
    if (same == NULL)
    {
        same = new(memCtx) Undef(); // The phi is unreachable or in the start block
    }
    phi->users.includes(phi);
    phi->users.remove(); // Remember all users except the phi itself
    phi->replaceBy(same); // Reroute all uses of phi to same and remove phi
    
    // Try to recursively remove all phi users, which might have become trivial
    CList<Phi> phis;
    foreach_list(phi->users, Instruction, iter)
    {
        Instruction *use = iter.value();
        if (use->op == OP_PHI)
        {
            phis.insertAfter((Phi*)use, NULL);
        }
    }
    foreach_list(phis, Phi, iter)
    {
        tryRemoveTrivialPhi(iter.value());
    }
    return same;
}

void SSABuilder::sealBlock(BasicBlock *block)
{
    foreach_list(block->incompletePhis, Phi, iter)
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
    Instruction *start = new(memCtx) NoOp(), *end = new(memCtx) NoOp();
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
}

void SSABuilder::printInstructionList()
{
    foreach_list(instructionList, Instruction, iter)
    {
        iter.value()->print();
    }
}

// predecessor: the basic block right before the loop
Loop::Loop(SSABuilder *bld, BasicBlock *predecessor)
{
    // create basic blocks
    loopEntry = bld->createBBAfter(predecessor);
    loopHeader = bld->createBBAfter(loopEntry);
    bodyEntry = bld->createBBAfter(loopHeader);
    bodyExit = bld->createBBAfter(bodyEntry);
    loopExit = bld->createBBAfter(bodyExit);
    
    // set predecessors
    loopEntry->addPred(predecessor);
    loopHeader->addPred(loopEntry);
    loopHeader->addPred(bodyExit);
    bodyEntry->addPred(loopHeader);
    loopExit->addPred(loopHeader);

    // seal blocks
    bld->sealBlock(loopEntry);
    bld->sealBlock(bodyEntry);
}

// used for constant folding
Constant *SSABuildUtil::applyOp(OpCode op, Constant *src0, Constant *src1)
{
    ScriptVariant *a = &src0->constValue,
                  *b = src1 ? &src1->constValue : NULL,
                  *result;
    switch(op)
    {
    case OP_NEG: result = ScriptVariant_Neg(a); break;
    case OP_BOOL_NOT: result = ScriptVariant_Boolean_Not(a); break;
    case OP_BIT_NOT: result = ScriptVariant_Bitwise_Not(a); break;
    default: result = NULL;
    }
    if (result && result->vt != VT_EMPTY)
        return new(builder->memCtx) Constant(*result);
    else
        return NULL;
}

RValue *SSABuildUtil::mkUnaryOp(OpCode op, RValue *src, BasicBlock *bb)
{
    RValue *result = NULL;
    if (src->isConstant()) // pre-evaluate expression
        result = applyOp(op, static_cast<Constant*>(src), NULL);
    if (result == NULL) // non-constant operands
        result = new(builder->memCtx) RValue(op, src);
    assert(result != NULL);
    builder->insertInstruction(result, bb);
    return result;
}

// TODO
RValue *SSABuildUtil::mkBinaryOp(OpCode op, RValue *src0, RValue *src1, BasicBlock *bb)
{
    return NULL;
}

// temporary
int main(int argc, char **argv)
{
    SSABuilder bld;
    SSABuildUtil bldUtil(&bld);
    BasicBlock *startBlock = bld.createBBAfter(NULL);
    RValue *const1, *const2, *nonConst1;
    bld.insertInstruction(nonConst1 = new(bld.memCtx) RValue(OP_CALL), startBlock);
    bld.insertInstruction(const1 = new(bld.memCtx) Constant(3), startBlock);
    bld.insertInstruction(const2 = new(bld.memCtx) Constant(5), startBlock);
    bldUtil.mkUnaryOp(OP_NEG, const1, startBlock);
    bldUtil.mkUnaryOp(OP_BIT_NOT, nonConst1, startBlock);
    bld.printInstructionList();
    return 0;
}

