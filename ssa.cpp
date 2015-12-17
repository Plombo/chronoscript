#include "ralloc.h"
#include "List.h"
#include <string.h>
#include <assert.h>

#include "ssa.h"

#if SSA_MAIN
#define ssaMain main
#endif

// referenced by another instruction
void RValue::ref(Instruction *inst)
{
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
    printf("%%r%i", id);
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
    printf("%s ", opCodeNames[op]);
    printOperands();
    printf("\n");
}

Expression::Expression(OpCode opCode, int valueId, RValue *src0, RValue *src1)
    : Instruction(opCode)
{
    dst = new(this) Temporary(valueId);
    //snprintf(dst->name, sizeof(dst->name), "%%r%i", valueId);
    if (src0)
        appendOperand(src0);
    if (src1)
        appendOperand(src1);
}

void Expression::print()
{
    if (dst->users.size() == 0) printf("[dead] ");
    dst->printDst();
    printf(" := ");
    printf("%s ", opCodeNames[op]);
    printOperands();
    printf("\n");
}

FunctionCall::FunctionCall(const char *function, int valueId)
    : Expression(OP_CALL, valueId)
{
    this->functionName = ralloc_strdup(this, function);
}

void FunctionCall::print()
{
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

Jump::Jump(OpCode opCode, BasicBlock *target, RValue *condition)
    : Instruction(opCode), target(target)
{
    if (condition)
        appendOperand(condition);
}

void Jump::print()
{
    printf("%s ", opCodeNames[op]);
    if (!operands.isEmpty()) // branch (conditional jump)
    {
        operands.retrieve()->printDst();
        printf(" ");
    }
    printf("=> ");
    target->printName();
    printf("\n");
}

void BlockDecl::print()
{
    printf("%s ", opCodeNames[op]);
    block->print();
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
    return (lastInstruction->op == OP_JMP || lastInstruction->op == OP_RETURN);
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
        if (op == same || op == phi->value())
            continue; // Unique value or self−reference
        if (same != NULL)
            return phi->value(); // The phi merges at least two values: not trivial
        same = op;
    }
    if (same == NULL)
    {
        same = new(memCtx) Undef(); // The phi is unreachable or in the start block
    }
    phi->value()->replaceBy(same); // Reroute all uses of phi to same and remove phi
    
    // Try to recursively remove all phi users, which might have become trivial
    CList<Phi> phis;
    foreach_list(phi->value()->users, Instruction, iter)
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
}

// insert instruction at start of basic block
void SSABuilder::insertInstructionAtStart(Instruction *inst, BasicBlock *bb)
{
    instructionList.setCurrent(bb->start);
    instructionList.insertAfter(inst, NULL);
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

SSABuildUtil::SSABuildUtil(SSABuilder *builder)
    : builder(builder), currentBlock(NULL)
{
    StackedSymbolTable_Init(&symbolTable);
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

Jump *SSABuildUtil::mkJump(OpCode op, BasicBlock *target, RValue *condition)
{
    Jump *inst = new(builder->memCtx) Jump(op, target, condition);
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
    if (!found) return false;
    if (!value->isTemporary()) // need a temporary for phi functions
        value = mkMove(value);
    builder->writeVariable(varName, currentBlock, value);
    return true;
}

// returns an Undef if varName is invalid in current scope
RValue *SSABuildUtil::readVariable(const char *varName)
{
    char scopedName[MAX_STR_LEN * 2 + 1];
    Symbol *sym;
    bool found = StackedSymbolTable_FindSymbol(&symbolTable, varName, &sym, scopedName);
    if (!found) return undef();
    RValue *value = builder->readVariable(varName, currentBlock);
    return value;
}

void SSABuildUtil::pushScope()
{
    StackedSymbolTable_PushScope(&symbolTable);
}

void SSABuildUtil::popScope()
{
    StackedSymbolTable_PopScope(&symbolTable);
}

// temporary
int ssaMain(int argc, char **argv)
{
    char str[64] = {"string"};
    void *memCtx = ralloc_context(NULL);
    SSABuilder bld = SSABuilder(memCtx);
    SSABuildUtil bldUtil = SSABuildUtil(&bld);
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

