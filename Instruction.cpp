#include <stdio.h>
#include <stdlib.h>
#include "SSABuilder.hpp"
#include "ScriptUtils.h"

// empty virtual destructor to silence compiler warnings
RValue::~RValue()
{
}

// referenced by another instruction
void RValue::ref(Instruction *inst)
{
    // if (isTemporary()) { printDst(); printf(" ref'd by %s\n", getOpCodeName(inst->op)); }
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
        // if (isTemporary()) { printDst(); printf(" unref'd by %s\n", getOpCodeName(inst->op)); }
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

Constant::Constant(int32_t intVal)
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
    else if (constValue.vt == VT_EMPTY)
    {
        printf("NULL");
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
            foreach_list(expr->operands, RValue*, iter)
            {
                if (!iter.value()->isBoolValue())
                    return false;
            }
            return true;
        default:
            return false;
    }
}

// empty virtual destructor to silence compiler warnings
Instruction::~Instruction()
{
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
    foreach_list(operands, RValue*, iter)
    {
        iter.value()->printDst();
        if (iter.hasNext()) printf(", ");
    }
}

void Instruction::print()
{
    if (seqIndex >= 0) printf("%i: ", seqIndex);
    printf("%s ", getOpCodeName(op));
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
    printf("%s ", getOpCodeName(op));
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
    printf("%s %s ", getOpCodeName(op), functionName);
    foreach_list(operands, RValue*, iter)
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
    printf("%s ", getOpCodeName(op));
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
    printf("%s ", getOpCodeName(op));
    block->print();
    printf("\n");
}

void Export::print()
{
    if (seqIndex >= 0) printf("%i: ", seqIndex);
    dst->printDst();
    printf(" := %s ", getOpCodeName(op));
    operands.retrieve()->printDst();
    printf("\n");
}

const char *getOpCodeName(OpCode op)
{
    switch (op)
    {
        case OP_NOOP:                return "noop";
        case OP_BB_START:            return "bb_start";
        case OP_PHI:                 return "phi";

        case OP_JMP:                 return "jmp";
        case OP_BRANCH_FALSE:        return "branch_false";
        case OP_BRANCH_TRUE:         return "branch_true";
        case OP_BRANCH_EQUAL:        return "branch_equal";
        case OP_RETURN:              return "return";

        case OP_MOV:                 return "mov";
        case OP_GET_GLOBAL:          return "get_global";

        case OP_NEG:                 return "neg";
        case OP_BOOL_NOT:            return "bool_not";
        case OP_BIT_NOT:             return "bit_not";
        case OP_BOOL:                return "bool";

        case OP_BIT_OR:              return "bit_or";
        case OP_XOR:                 return "xor";
        case OP_BIT_AND:             return "bit_and";
        case OP_EQ:                  return "eq";
        case OP_NE:                  return "ne";
        case OP_LT:                  return "lt";
        case OP_GT:                  return "gt";
        case OP_GE:                  return "ge";
        case OP_LE:                  return "le";
        case OP_SHL:                 return "shl";
        case OP_SHR:                 return "shr";
        case OP_ADD:                 return "add";
        case OP_SUB:                 return "sub";
        case OP_MUL:                 return "mul";
        case OP_DIV:                 return "div";
        case OP_REM:                 return "rem";

        case OP_CALL:                return "call";
        case OP_CALL_BUILTIN:        return "call_builtin";
        case OP_CALL_METHOD:         return "call_method";

        case OP_MKOBJECT:            return "mkobject";
        case OP_MKLIST:              return "mklist";
        case OP_GET:                 return "get";
        case OP_SET:                 return "set";

        case OP_EXPORT:              return "export";

        case OP_ERR:                 return "???";
    }

    return "???";
}


