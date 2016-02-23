#include "ralloc.h"
#include "List.h"
#include <string.h>
#include <assert.h>

#include "ssa.h"
#include "regalloc.h"

#if SSA_MAIN
#define ssaMain main
#endif

// referenced by another instruction
void RValue::ref(Instruction *inst)
{
    // if (isTemporary()) { printDst(); printf(" ref'd by %s\n", opCodeNames[inst->op]); }
    users.gotoLast();
    users.insertAfter(inst, NULL);
}

// unreferenced by other instruction
void RValue::unref(Instruction *inst)
{
    // inst will be in users list more than once if it references
    // this value more than once; only remove one instance
    if (users.includes(inst))
    {
        users.remove();
        // if (isTemporary()) { printDst(); printf(" unref'd by %s\n", opCodeNames[inst->op]); }
    }
}

void RValue::replaceBy(RValue *newValue)
{
    while (!users.isEmpty())
    {
        Instruction *inst = users.retrieve();
        int numSrc = inst->operands.size();
        for (int i = 0; i < numSrc; i++)
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

bool RValue::isConstant()
{
    return false;
}

bool RValue::isTemporary()
{
    return false;
}

bool RValue::isUndefined()
{
    return false;
}

bool Undef::isUndefined()
{
    return true;
}

void Undef::printDst()
{
    printf("(UNDEF)");
}

bool Temporary::isTemporary()
{
    return true;
}

void Temporary::printDst()
{
    if (reg >= 0)
        printf("$%i", reg);
    else
        printf("%%r%i", id);
}

void GlobalVarRef::printDst()
{
    printf("@%i", id);
}

Constant::Constant(ScriptVariant val)
{
    constValue = val;
}

Constant::Constant(LONG intVal)
{
    constValue.vt = VT_INTEGER;
    constValue.lVal = intVal;
}

bool Constant::isConstant()
{
    return true;
}

void Constant::printDst()
{
    if (constValue.vt == VT_INTEGER)
    {
        printf("%i", constValue.lVal);
    }
    else if (constValue.vt == VT_DECIMAL)
    {
        printf("%f", constValue.dblVal);
    }
    else if (constValue.vt == VT_STR)
    {
        // FIXME: re-escape characters such as '\r' and '\n'
        printf("\"%s\"", StrCache_Get(constValue.strVal));
    }
    else
    {
        printf("const[?]");
    }
}

void Param::printDst()
{
    printf("param[%i]", index);
}

void Instruction::appendOperand(RValue *value)
{
    operands.gotoLast();
    operands.insertAfter(value);
    value->ref(this);
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
    operands.gotoFirst();
    for(int i=0; i<index; i++) operands.gotoNext();
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

void Instruction::printOperands()
{
    foreach_list(operands, RValue, iter)
    {
        iter.value()->printDst();
        if (iter.hasNext()) printf(", ");
    }
}

void Instruction::print()
{
    if (seqIndex >= 0) printf("%i: ", seqIndex);
    printf("%s ", opCodeNames[op]);
    printOperands();
    printf("\n");
}

bool Instruction::isExpression()
{
    return false;
}

bool Instruction::isJump()
{
    return false;
}

bool Instruction::isTrivial()
{
    return false;
}

Expression::Expression(OpCode opCode, int valueId, RValue *src0, RValue *src1)
    : Instruction(opCode)
{
    dst = new(this) Temporary(valueId, this);
    //snprintf(dst->name, sizeof(dst->name), "%%r%i", valueId);
    if (src0)
        appendOperand(src0);
    if (src1)
        appendOperand(src1);
}

bool Expression::isDead()
{
    return dst->isDead();
}

void Expression::print()
{
    if (isDead()) printf("[dead] ");
    if (seqIndex >= 0) printf("%i: ", seqIndex);
    dst->printDst();
    printf(" := ");
    printf("%s ", opCodeNames[op]);
    printOperands();
    printf("\n");
}

bool Expression::isExpression()
{
    return true;
}

bool Expression::isTrivial()
{
    if (op == OP_MOV && dst->isTemporary() && operands.retrieve()->isTemporary())
    {
        if (static_cast<Temporary*>(dst)->reg == static_cast<Temporary*>(operands.retrieve())->reg)
            return true;
    }
    return false;
}

bool Phi::isTrivial()
{
    return true;
}

FunctionCall::FunctionCall(const char *function, int valueId)
    : Expression(OP_CALL, valueId)
{
    this->functionName = ralloc_strdup(this, function);
}

bool FunctionCall::isDead()
{
    return false;
}

void FunctionCall::print()
{
    if (seqIndex >= 0) printf("%i: ", seqIndex);
    dst->printDst();
    printf(" := ");
    printf("%s %s ", opCodeNames[op], functionName);
    foreach_list(operands, RValue, iter)
    {
        iter.value()->printDst();
        if (iter.hasNext()) printf(", ");
    }
    printf("\n");
}

Jump::Jump(OpCode opCode, BasicBlock *target, RValue *src0, RValue *src1)
    : Instruction(opCode), target(target)
{
    if (src0)
        appendOperand(src0);
    if (src1)
        appendOperand(src1);
}

void Jump::print()
{
    if (seqIndex >= 0) printf("%i: ", seqIndex);
    printf("%s ", opCodeNames[op]);
    printOperands();
    if (!operands.isEmpty()) printf(" ");
    printf("=> ");
    if (target->startIndex >= 0)
        printf("%i", target->startIndex);
    else
        target->printName();
    printf("\n");
}

bool Jump::isJump()
{
    return true;
}

bool NoOp::isTrivial()
{
    return true;
}

void BlockDecl::print()
{
    if (seqIndex >= 0) printf("%i: ", seqIndex);
    printf("%s ", opCodeNames[op]);
    block->print();
    printf("\n");
}

void Export::print()
{
    if (seqIndex >= 0) printf("%i: ", seqIndex);
    dst->printDst();
    printf(" := %s ", opCodeNames[op]);
    operands.retrieve()->printDst();
    printf("\n");
}

void BasicBlock::addPred(BasicBlock *newPred)
{
    preds.gotoLast();
    preds.insertAfter(newPred);
}

// returns true if last instruction in block is an unconditional jump (JMP/RETURN)
bool BasicBlock::endsWithJump()
{
    Instruction *lastInstruction = static_cast<Instruction*>(end->prev->value);
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
    foreach_list(preds, BasicBlock, iter)
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
    foreach_list(b->preds, BasicBlock, iter)
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
    foreach_list(block->preds, BasicBlock, iter)
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
    printf("tryRemoveTrivialPhi: "); phi->print(); 
    RValue *same = NULL;
    foreach_list(phi->operands, RValue, iter)
    {
        RValue *op = iter.value();
        if (op == same || op == phi->value())
            continue; // Unique value or self−reference
        if (same != NULL) {
            printf("not trivial: "); same->printDst(); printf(" and "); op->printDst(); printf("\n");
            return phi->value(); // The phi merges at least two values: not trivial
        }
        same = op;
    }
    if (same == NULL)
    {
        same = new(memCtx) Undef(); // The phi is unreachable or in the start block
    }
    printf("trivial phi; replace with "); same->printDst(); printf("\n");
    // remember all users except the phi itself
    CList<Phi> phiUsers;
    foreach_list(phi->value()->users, Instruction, iter)
    {
        Instruction *use = iter.value();
        if (use->isPhi() && use != phi)
        {
            phiUsers.insertAfter(static_cast<Phi*>(use), NULL);
        }
    }
    phi->value()->replaceBy(same); // Reroute all uses of phi to same and remove phi
    foreach_list(currentDef, RValue, iter)
    {
        // replace variable value with same
        if (iter.value() == phi->value())
            iter.update(same);
    }
    
    // Try to recursively remove all phi users, which might have become trivial
    foreach_list(phiUsers, Phi, iter)
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
    foreach_list(instructionList, Instruction, iter)
    {
        iter.value()->print();
    }
}

// returns true if dead code was removed
// XXX: this would be more efficient with an iterator that lets us delete members
bool SSABuilder::removeDeadCode()
{
    bool codeRemoved = false;
    instructionList.gotoFirst();
    int i, size = instructionList.size();
    for (i = 0; i < size; i++)
    {
        Instruction *inst = instructionList.retrieve();
        if (inst->isExpression())
        {
            Expression *expr = static_cast<Expression*>(inst);
            if (expr->isDead())
            {
                // RValue *undefined = new(memCtx) Undef();
                foreach_list(expr->operands, RValue, iter)
                {
                    iter.value()->unref(expr);
                }
                instructionList.remove();
                codeRemoved = true;
                continue;
            }
        }
        instructionList.gotoNext();
    }
    return codeRemoved;
}

void SSABuilder::prepareForRegAlloc()
{
    // insert phi moves
    foreach_list(basicBlockList, BasicBlock, iter)
    {
        BasicBlock *block = iter.value();
        Node *instNode = block->start;
        while (instNode = instNode->next)
        {
            Instruction *inst = (Instruction*) instNode->value;
            if (inst->op != OP_PHI) break; // only process phis, which are always at start of block
            Phi *phi = (Phi*) inst;
            // phi->print();
            // insert moves for phi before last jump in src block
            int i = 0;
            foreach_list(phi->operands, RValue, srcIter)
            {
                RValue *phiSrc = srcIter.value();
                BasicBlock *srcBlock = phi->sourceBlocks[i++];
                Node *insertPoint = srcBlock->end->prev;
                Expression *move = new(memCtx) Expression(OP_MOV, valueId(), phiSrc);
                move->block = srcBlock;
                while (((Instruction*)insertPoint->value)->isJump()) insertPoint = insertPoint->prev;
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
                foreach_list(phiSrc->users, Instruction, refIter)
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
                        foreach_list(inst2->operands, RValue, srcIter2)
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
        foreach_list(block->preds, BasicBlock, predIter)
        {
            predIter.value()->succs.insertAfter(block, NULL);
        }
    }
    
    // build temporary list and assign sequential indices to instructions
    assert(temporaries.isEmpty());
    int tempCount = 0, instCount = 0;
    foreach_list(instructionList, Instruction, iter)
    {
        iter.value()->seqIndex = instCount++;
        if (!iter.value()->isExpression()) continue;
        Expression *expr = static_cast<Expression*>(iter.value());
        if (!expr->value()->isTemporary()) continue;
        Temporary *temp = static_cast<Temporary*>(expr->value());
        temp->id = tempCount++; // renumber temporaries
        temporaries.insertAfter(temp);
    }

    // build basic block live sets
    foreach_list(basicBlockList, BasicBlock, iter)
    {
        BasicBlock *block = iter.value();
        block->liveIn.allocate(tempCount, true);
        block->liveOut.allocate(tempCount, true);
        block->phiDefs.allocate(tempCount, true);
        block->phiUses.allocate(tempCount, true);
        block->processed = false;
    }

    // build phiUses and phiDefs sets
    foreach_list(basicBlockList, BasicBlock, iter)
    {
        BasicBlock *block = iter.value();
        Node *instNode = block->start;
        while (instNode = instNode->next)
        {
            Instruction *inst = (Instruction*) instNode->value;
            if (inst->op != OP_PHI) break; // only process phis, which are always at start of block
            Phi *phi = (Phi*) inst;
            // update phiDefs with phi definition
            block->phiDefs.set(static_cast<Temporary*>(phi->value())->id);
            foreach_list(phi->operands, RValue, srcIter)
            {
                // update phiUses for each phi operand
                Temporary *phiSrc = static_cast<Temporary*>(srcIter.value());
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

// returns true on success, false if name already in use
bool GlobalState::declareGlobalVariable(const char *varName)
{
    if (globalVariables.findByName(varName))
    {
        // name already defined
        printf("Error: global variable %s redefined\n", varName);
        return false;
    }
    globalVariables.gotoLast();
    globalVariables.insertAfter(NULL, varName);
    printf("Declare global variable '%s'\n", varName);
    return true;
}

// bool GlobalState::initializeGlobalVariable(const char *varName, ScriptVariant *value)
// {
// }

GlobalVarRef *GlobalState::readGlobalVariable(const char *varName, void *memCtx)
{
    printf("Try to read global variable '%s'\n", varName);
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
    StackedSymbolTable_Init(&symbolTable);
    currentLoop = NULL;
}

SSABuildUtil::~SSABuildUtil()
{
    // this will free all of the symbols we've allocated
    StackedSymbolTable_Clear(&symbolTable);
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
    ScriptVariant *result;
    switch(op)
    {
    // unary
    case OP_NEG: result = ScriptVariant_Neg(src0); break;
    case OP_BOOL_NOT: result = ScriptVariant_Boolean_Not(src0); break;
    case OP_BIT_NOT: result = ScriptVariant_Bit_Not(src0); break;
    // binary
    case OP_BOOL_OR: result = ScriptVariant_Or(src0, src1); break;
    case OP_BOOL_AND: result = ScriptVariant_And(src0, src1); break;
    case OP_BIT_OR: result = ScriptVariant_Bit_Or(src0, src1); break;
    case OP_XOR: result = ScriptVariant_Xor(src0, src1); break;
    case OP_BIT_AND: result = ScriptVariant_Bit_And(src0, src1); break;
    case OP_EQ: result = ScriptVariant_Eq(src0, src1); break;
    case OP_NE: result = ScriptVariant_Ne(src0, src1); break;
    case OP_LT: result = ScriptVariant_Lt(src0, src1); break;
    case OP_GT: result = ScriptVariant_Gt(src0, src1); break;
    case OP_GE: result = ScriptVariant_Ge(src0, src1); break;
    case OP_LE: result = ScriptVariant_Le(src0, src1); break;
    case OP_SHL: result = ScriptVariant_Shl(src0, src1); break;
    case OP_SHR: result = ScriptVariant_Shr(src0, src1); break;
    case OP_ADD: result = ScriptVariant_Add(src0, src1); break;
    case OP_SUB: result = ScriptVariant_Add(src0, src1); break;
    case OP_MUL: result = ScriptVariant_Mul(src0, src1); break;
    case OP_DIV: result = ScriptVariant_Div(src0, src1); break;
    case OP_MOD: result = ScriptVariant_Mod(src0, src1); break;
    default: result = NULL;
    }
    if (result && result->vt != VT_EMPTY)
    {
        return builder->addConstant(*result);
    }
    return NULL;
}

RValue *SSABuildUtil::mkUnaryOp(OpCode op, RValue *src)
{
    RValue *result = NULL;
    if (src->isConstant()) // pre-evaluate expression
    {
        result = applyOp(op, &static_cast<Constant*>(src)->constValue, NULL);
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
                         &static_cast<Constant*>(src0)->constValue,
                         &static_cast<Constant*>(src1)->constValue);
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

Constant *SSABuildUtil::mkConstInt(LONG val)
{
    ScriptVariant var = {{.lVal = val}, VT_INTEGER};
    return builder->addConstant(var);
}

Constant *SSABuildUtil::mkConstString(char *val)
{
    ScriptVariant var = {{.ptrVal = NULL}, VT_EMPTY};
    ScriptVariant_ParseStringConstant(&var, val);
    return builder->addConstant(var);
}

Constant *SSABuildUtil::mkConstFloat(double val)
{
    ScriptVariant var = {{.dblVal = val}, VT_DECIMAL};
    return builder->addConstant(var);
}

Constant *SSABuildUtil::mkNull()
{
    ScriptVariant var = {{.ptrVal = NULL}, VT_EMPTY};
    return builder->addConstant(var);
}

Jump *SSABuildUtil::mkJump(OpCode op, BasicBlock *target, RValue *src0, RValue *src1)
{
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

// inserts a finished function call instruction at the end of the current block
void SSABuildUtil::insertFunctionCall(FunctionCall *call)
{
    builder->insertInstruction(call, currentBlock);
}

Undef *SSABuildUtil::undef() // get an undefined value
{
    return new(builder->memCtx) Undef();
}

void SSABuildUtil::declareVariable(const char *varName)
{
    Symbol *sym = (Symbol *) malloc(sizeof(Symbol));
    Symbol_Init(sym, varName, NULL);
    StackedSymbolTable_AddSymbol(&symbolTable, sym);
    // TODO duplicate declarations?
}

// returns true if varName is valid in current scope
bool SSABuildUtil::writeVariable(const char *varName, RValue *value)
{
    char scopedName[MAX_STR_LEN * 2 + 1];
    Symbol *sym;
    bool found = StackedSymbolTable_FindSymbol(&symbolTable, varName, &sym, scopedName);
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

// returns an Undef if varName is invalid in current scope
RValue *SSABuildUtil::readVariable(const char *varName)
{
    char scopedName[MAX_STR_LEN * 2 + 1];
    Symbol *sym;
    bool found = StackedSymbolTable_FindSymbol(&symbolTable, varName, &sym, scopedName);
    if (found)
    {
        RValue *value = builder->readVariable(varName, currentBlock);
        return value;
    }
    else
    {
        // it's either a global variable or undefined
        GlobalVarRef *global = globalState->readGlobalVariable(varName, builder->memCtx);
        if (global) return mkMove(global);
        else return undef();
    }
}

void SSABuildUtil::pushScope()
{
    StackedSymbolTable_PushScope(&symbolTable);
}

void SSABuildUtil::popScope()
{
    StackedSymbolTable_PopScope(&symbolTable);
}

void SSABuildUtil::pushLoop(Loop *loop)
{
    loop->parent = currentLoop;
    if (currentLoop == NULL) // this is a top-level loop
        builder->loops.insertAfter(loop);
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
    block->loop = loop ? loop : currentLoop;
    return block;
}

// temporary
int ssaMain(int argc, char **argv)
{
    char str[64] = {"string"};
    void *memCtx = ralloc_context(NULL);
    SSABuilder bld = SSABuilder(memCtx);
    GlobalState globalState;
    SSABuildUtil bldUtil = SSABuildUtil(&bld, &globalState);
    BasicBlock *startBlock = bld.createBBAfter(NULL);
    bld.sealBlock(startBlock);
    bldUtil.setCurrentBlock(startBlock);
    Expression *nonConst1;
    bld.insertInstruction(nonConst1 = new(bld.memCtx) Expression(OP_CALL, bld.valueId()), startBlock);
    Constant *const1 = bldUtil.mkConstInt(3);
    Constant *const2 = bldUtil.mkConstInt(5);
    Constant *const3 = bldUtil.mkConstString(str);
    Constant *const4 = bldUtil.mkConstFloat(4.1);
    
    #define RSIZE 11
    RValue *r[RSIZE];
    memset(r, 0, sizeof(r));
    
    // declare "bar" as a parameter
    bldUtil.addParam("bar");
    
    // entry basic block
    r[0] = bldUtil.mkUnaryOp(OP_NEG, const1);
    r[1] = bldUtil.mkUnaryOp(OP_BIT_NOT, nonConst1->value());
    r[2] = bldUtil.mkBinaryOp(OP_ADD, const1, const2);
    r[3] = bldUtil.mkBinaryOp(OP_ADD, const1, nonConst1->value());
    r[4] = bldUtil.mkBinaryOp(OP_BOOL_AND, const1, const2);
    r[5] = bldUtil.mkBinaryOp(OP_ADD, const1, const3);
    r[6] = bldUtil.mkBinaryOp(OP_ADD, const1, const4);

    // int foo;
    bldUtil.declareVariable("foo");
    //bldUtil.declareVariable("bar");
    
    BasicBlock *ifBlock = bld.createBBAfter(startBlock),
               *endIfBlock = bld.createBBAfter(ifBlock),
               *afterIfBlock;
    ifBlock->addPred(startBlock);
    bld.sealBlock(ifBlock);

    // if (r[2]) {
    Jump *skipIfJump = bldUtil.mkJump(OP_BRANCH_FALSE, NULL, r[2]);
    bldUtil.pushScope();
    bldUtil.setCurrentBlock(ifBlock);
    // foo = r[2] + r[1];
    r[7] = bldUtil.mkBinaryOp(OP_ADD, r[2], r[1]);
    bldUtil.writeVariable("foo", r[7]);
    bldUtil.writeVariable("bar", r[0]);
    // }
    endIfBlock->addPred(bldUtil.currentBlock); // == ifBlock, in this simple case
    bld.sealBlock(endIfBlock);
    bldUtil.popScope();
    
    bool hasElse = true;
    if (hasElse)
    {
        // else {
        BasicBlock *elseBlock = bld.createBBAfter(endIfBlock);
        afterIfBlock = bld.createBBAfter(elseBlock);
        elseBlock->addPred(startBlock);
        bld.sealBlock(elseBlock);
        skipIfJump->target = elseBlock;
        bldUtil.setCurrentBlock(elseBlock);

        // foo = r[0] + r[1];
        r[8] = bldUtil.mkBinaryOp(OP_ADD, r[0], const2);
        bldUtil.writeVariable("foo", r[8]);
        // }
        afterIfBlock->addPred(bldUtil.currentBlock); // == elseBlock, in this simple case
        bldUtil.setCurrentBlock(endIfBlock);
        Jump *skipElseJump = bldUtil.mkJump(OP_JMP, afterIfBlock, NULL);
        afterIfBlock->addPred(endIfBlock);
        bld.sealBlock(afterIfBlock);
    }
    else
    {
        skipIfJump->target = endIfBlock;
        afterIfBlock = endIfBlock;
    }
    // common to both if-only and if-else
    bldUtil.setCurrentBlock(afterIfBlock);

    // now read the "foo" variable
    r[9] = bldUtil.readVariable("foo");
    r[10] = bldUtil.readVariable("bar"); // this one has an undefined path

    bld.printInstructionList();
    for (int i=0; i < RSIZE; i++)
    {
        printf("\n%i = ", i);
        if (r[i]) r[i]->printDst();
        else printf("(none)");
    }
    
    ralloc_free(memCtx);
    return 0;
}

