#include "ralloc.h"
#include "List.h"
#include <string.h>
#include <assert.h>

#include "ScriptUtils.h"
#include "ssa.h"
#include "regalloc.h"

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

bool RValue::isParam()
{
    return false;
}

bool RValue::isGlobalVarRef()
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

bool GlobalVarRef::isGlobalVarRef()
{
    return true;
}

void GlobalVarRef::printDst()
{
    printf("@%i", id);
}

Constant::Constant(ScriptVariant val)
{
    id = -1;
    constValue = val;
}

Constant::Constant(s32 intVal)
{
    id = -1;
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
        printEscapedString(StrCache_Get(constValue.strVal));
    }
    else
    {
        printf("const[?]");
    }
}

bool Param::isParam()
{
    return true;
}

void Param::printDst()
{
    printf("param[%i]", index);
}

// isBoolValue: true if this value is guaranteed to be int 0 or int 1
bool RValue::isBoolValue()
{
    return false;
}

bool Constant::isBoolValue()
{
    return (constValue.vt == VT_INTEGER && (constValue.lVal == 0 || constValue.lVal == 1));
}

bool Temporary::isBoolValue()
{
    switch(expr->op)
    {
        // these operations will always have an integer result of 0 or 1
        case OP_BOOL:
        case OP_BOOL_NOT:
        case OP_EQ:
        case OP_NE:
        case OP_LT:
        case OP_GT:
        case OP_GE:
        case OP_LE:
            return true;
        case OP_PHI:
            if (!expr->block->isSealed) return false;
            foreach_list(expr->operands, RValue, iter)
            {
                if (!iter.value()->isBoolValue())
                    return false;
            }
            return true;
        default:
            return false;
    }
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
        if (dst->asTemporary()->reg == operands.retrieve()->asTemporary()->reg)
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

const char *SSAFunction::getIdentString(const char *variable, BasicBlock *block)
{
    snprintf(identBuf, sizeof(identBuf), "%s:%i", variable, block->id);
    return identBuf;
}

void SSAFunction::writeVariable(const char *variable, BasicBlock *block, RValue *value)
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

RValue *SSAFunction::readVariable(const char *variable, BasicBlock *block)
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

RValue *SSAFunction::readVariableRecursive(const char *variable, BasicBlock *block)
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

RValue *SSAFunction::addPhiOperands(const char *variable, BasicBlock *block, Phi *phi)
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

RValue *SSAFunction::tryRemoveTrivialPhi(Phi *phi)
{
    // printf("tryRemoveTrivialPhi: "); phi->print();
    RValue *same = NULL;
    foreach_list(phi->operands, RValue, iter)
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
    CList<Phi> phiUsers;
    foreach_list(phi->value()->users, Instruction, iter)
    {
        Instruction *use = iter.value();
        if (use->isPhi() && use != phi)
        {
            phiUsers.insertAfter(use->asPhi(), NULL);
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

void SSAFunction::sealBlock(BasicBlock *block)
{
    foreach_list(block->incompletePhis, Phi, iter)
    {
        addPhiOperands(iter.name(), block, iter.value());
    }
    block->isSealed = true; // sealedBlocks.add(block)
}

// create BB and insert after the given block
BasicBlock *SSAFunction::createBBAfter(BasicBlock *existingBB)
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
void SSAFunction::insertInstruction(Instruction *inst, BasicBlock *bb)
{
    instructionList.setCurrent(bb->end);
    instructionList.insertBefore(inst, NULL);
    inst->block = bb;
}

// insert instruction at start of basic block
void SSAFunction::insertInstructionAtStart(Instruction *inst, BasicBlock *bb)
{
    instructionList.setCurrent(bb->start);
    instructionList.insertAfter(inst, NULL);
    inst->block = bb;
}

// insert instruction at end of basic block
Constant *SSAFunction::addConstant(ScriptVariant sv)
{
    // TODO: prevent duplicates
    constantList.gotoLast();
    Constant *c = new(memCtx) Constant(sv);
    constantList.insertAfter(c, NULL);
    return c;
}

void SSAFunction::printInstructionList()
{
    foreach_list(instructionList, Instruction, iter)
    {
        iter.value()->print();
    }
}

// returns true if dead code was removed
// XXX: this would be more efficient with an iterator that lets us delete members
bool SSAFunction::removeDeadCode()
{
    bool codeRemoved = false;
    instructionList.gotoFirst();
    int i, size = instructionList.size();
    for (i = 0; i < size; i++)
    {
        Instruction *inst = instructionList.retrieve();
        if (inst->isExpression())
        {
            Expression *expr = inst->asExpression();
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

void SSAFunction::prepareForRegAlloc()
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
        Expression *expr = iter.value()->asExpression();
        if (!expr->value()->isTemporary()) continue;
        Temporary *temp = expr->value()->asTemporary();
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
            block->phiDefs.set(phi->value()->asTemporary()->id);
            foreach_list(phi->operands, RValue, srcIter)
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

SSABuildUtil::SSABuildUtil(SSAFunction *builder, GlobalState *globalState)
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
    case OP_BOOL: result = ScriptVariant_ToBoolean(src0); break;
    // binary
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

Constant *SSABuildUtil::mkConstInt(s32 val)
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

