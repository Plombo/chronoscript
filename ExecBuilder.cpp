#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "ExecBuilder.hpp"
#include "Builtins.hpp"
#include "ScriptUtils.h"

ExecBuilder::ExecBuilder(const char *filename)
{
    interpreter = new Interpreter(filename);
}

void ExecBuilder::allocateExecFunctions()
{
    // allocate an ExecFunction for each source function so that when one of
    // these functions calls another, we can properly link the call
    foreach_list(ssaFunctions, SSABuilder*, iter)
    {
        ExecFunction *execFunc = new ExecFunction;
        execFunc->numParams = iter.value()->paramCount;
        execFunc->functionName = strdup(iter.value()->functionName);
        interpreter->functions.insertAfter(execFunc, iter.name());
    }
}

void ExecBuilder::buildExecutable()
{
    // actually build the ExecFunctions
    foreach_list(ssaFunctions, SSABuilder*, iter)
    {
        FunctionBuilder builder(iter.value(), this);
        builder.run();
    }

    // build the constants array
    interpreter->numConstants = constants.size();
    interpreter->constants = new ScriptVariant[interpreter->numConstants];
    int i = 0;
    foreach_list(constants, ScriptVariant*, iter)
    {
        interpreter->constants[i++] = *iter.value();
    }

    // build the globals array
    interpreter->numGlobals = globals.globalVariables.size();
    interpreter->globals = new ScriptVariant[interpreter->numGlobals];

    // set all globals to their initial values
    i = 0;
    foreach_list(globals.globalVariables, ScriptVariant, iter)
    {
        interpreter->globals[i++] = iter.value();
    }
}

ExecFunction *ExecBuilder::getFunctionNamed(const char *name)
{
    if (interpreter->functions.findByName(name))
        return interpreter->functions.retrieve();
    else
        return NULL;
}

static uint16_t createSrc(RValue *src)
{
    uint8_t file = FILE_NONE, index = 0;
    if (src->isTemporary())
    {
        file = FILE_TEMP;
        index = src->asTemporary()->reg;
    }
    else if (src->isParam())
    {
        file = FILE_PARAM;
        index = src->asParam()->index;
    }
    else if (src->isGlobalVarRef())
    {
        file = FILE_GLOBAL;
        index = src->asGlobalVarRef()->id;
    }
    else if (src->isConstant())
    {
        int id = src->asConstant()->id;
        file = FILE_CONSTANT + (id / 256);
        index = id % 256;
    }
    return (file << 8) | index;
}

void FunctionBuilder::createExecInstruction(ExecInstruction *inst, Instruction *ssaInst)
{
    inst->opCode = ssaInst->op;
    if (ssaInst->isFunctionCall())
    {
        // call target
        FunctionCall *ssaCall = ssaInst->asFunctionCall();
        if (ssaInst->op == OP_CALL_BUILTIN)
        {
            inst->callTarget = ssaCall->builtinRef;
        }
        else
        {
            ExecFunction *target = ssaCall->functionRef;
            assert(target);
            inst->callTarget = nextCallTargetIndex;
            func->callTargets[nextCallTargetIndex++] = target;
        }

        int numParams = ssaInst->operands.size();
        assert(numParams < 256);
        if (numParams > func->maxCallParams)
            func->maxCallParams = numParams;
        inst->paramsIndex = nextParamIndex;
        func->callParams[nextParamIndex++] = numParams;
        foreach_list(ssaInst->operands, RValue*, iter)
        {
            func->callParams[nextParamIndex] = createSrc(iter.value());
            ++nextParamIndex;
        }
    }
    else
    {
        int numSrc = ssaInst->operands.size();
        assert(numSrc >= 0 && numSrc <= 3);
        if (numSrc >= 1) // src0
            inst->src0 = createSrc(ssaInst->src(0));
        if (numSrc >= 2) // src1
            inst->src1 = createSrc(ssaInst->src(1));
        if (numSrc >= 3) // src2
            inst->src2 = createSrc(ssaInst->src(2));
        else if (ssaInst->isJump()) // note: jumpTarget and src2 are aliased
            inst->jumpTarget = ssaInst->asJump()->target->startIndex;
    }

    // set dst
    if (ssaInst->isExpression())
        inst->dst = ssaInst->asExpression()->value()->reg;
    else if (ssaInst->op == OP_EXPORT)
        inst->dst = ssaInst->asExport()->dst->id;
}

FunctionBuilder::FunctionBuilder(SSABuilder *ssaFunc, ExecBuilder *builder)
    : execBuilder(builder), ssaFunc(ssaFunc), nextParamIndex(0), nextCallTargetIndex(0)
{
    this->func = execBuilder->getFunctionNamed(ssaFunc->functionName);
    assert(func);
}

void FunctionBuilder::run()
{
    func->interpreter = execBuilder->interpreter;
    func->numParams = ssaFunc->paramCount;
    func->maxCallParams = 0;

    int numTemps = 0, numCalls = 0, numParams = 0;
    foreach_list(ssaFunc->instructionList, Instruction*, iter)
    {
        Instruction *inst = iter.value();
        if (inst->isFunctionCall())
        {
            numParams += inst->operands.size() + 1;
            if (inst->op != OP_CALL_BUILTIN) ++numCalls;
        }
        foreach_list(inst->operands, RValue*, srcIter)
        {
            if (srcIter.value()->isTemporary())
            {
                int index = srcIter.value()->asTemporary()->reg;
                if (index >= numTemps)
                    numTemps = index + 1;
            }
        }
    }

    func->numInstructions = ssaFunc->instructionList.size();
    func->instructions = new ExecInstruction[func->numInstructions];
    memset(func->instructions, 0, func->numInstructions * sizeof(ExecInstruction));
    func->callTargets = new ExecFunction*[numCalls];
    func->callParams = new uint16_t[numParams];
    func->numTemps = numTemps;
    foreach_list(ssaFunc->instructionList, Instruction*, iter)
    {
        Instruction *inst = iter.value();
        if (inst->seqIndex < 0) continue;
        createExecInstruction(&(func->instructions[inst->seqIndex]), inst);
    }
}

static void printSrc(uint16_t src)
{
    int file = src >> 8, index = src & 0xff;
    if (file == FILE_NONE) return;
    else if (file > FILE_CONSTANT)
        printf("const%i", file - FILE_CONSTANT);
    else
        printf("%s", getRegisterFileName((RegFile) file));
    printf("[%i] ", index);
}

void ExecBuilder::printInstructions()
{
    // print constants
    printf("\nConstants: ");
    for (int i = 0; i < interpreter->numConstants; i++)
    {
        ScriptVariant *val = &interpreter->constants[i];
        if (val->vt == VT_INTEGER)
        {
            printf("%i", val->lVal);
        }
        else if (val->vt == VT_DECIMAL)
        {
            printf("%f", val->dblVal);
        }
        else if (val->vt == VT_STR)
        {
            printEscapedString(StrCache_Get(val->strVal));
        }
        else if (val->vt == VT_EMPTY)
        {
            printf("NULL");
        }
        else
        {
            printf("const[?]");
        }
        printf(" ");
    }
    printf("\n");
    // print each function
    foreach_list(interpreter->functions, ExecFunction*, funcIter)
    {
        ExecFunction *func = funcIter.value();
        printf("\n~~~ Function '%s' (%i params) ~~~\n", func->functionName, func->numParams);
        for (int i = 0; i < func->numInstructions; i++)
        {
            ExecInstruction *inst = &func->instructions[i];

            // instruction number
            printf("%i: ", i);

            // destination
            if (inst->opCode >= OP_MOV && inst->opCode <= OP_GET)
            {
                printf("temp[%i] := ", inst->dst);
            }
            else if (inst->opCode == OP_EXPORT)
            {
                printf("global[%i] := ", inst->dst);
            }

            // opcode
            printf("%s ", getOpCodeName((OpCode)inst->opCode));

            // parameters/sources
            if (inst->opCode == OP_CALL || inst->opCode == OP_CALL_BUILTIN)
            {
                const char *functionName = (inst->opCode == OP_CALL_BUILTIN)
                    ? getBuiltinName(inst->callTarget)
                    : func->callTargets[inst->callTarget]->functionName;
                printf("%s ", functionName);
                uint16_t paramCount16 = func->callParams[inst->paramsIndex];
                assert(paramCount16 >> 8 == FILE_NONE);
                int paramCount = paramCount16 & 0xff;
                for (int j = 0; j < paramCount; j++)
                {
                    printSrc(func->callParams[inst->paramsIndex+j+1]);
                }
            }
            else
            {
                printSrc(inst->src0);
                printSrc(inst->src1);
                printSrc(inst->src2);
            }

            // jump target
            if (inst->opCode >= OP_JMP && inst->opCode <= OP_BRANCH_EQUAL)
            {
                printf("=> %i", inst->jumpTarget);
            }

            printf("\n");
        }
    }
}
