#include "ExecBuilder.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

ExecBuilder::ExecBuilder()
{
    interpreter = new Interpreter;
}

void ExecBuilder::buildExecutable()
{
    // allocate an ExecFunction for each source function so that when one of
    // these functions calls another, we can properly link the call
    foreach_list(ssaFunctions, SSABuilder, iter)
    {
        ExecFunction *execFunc = new ExecFunction;
        interpreter->functions.insertAfter(execFunc, iter.name());
    }

    // now actually build the ExecFunctions
    foreach_list(ssaFunctions, SSABuilder, iter)
    {
        FunctionBuilder builder(iter.value(), this);
        builder.run();
    }

    // build the constants array
    interpreter->numConstants = constants.size();
    interpreter->constants = new ScriptVariant[interpreter->numConstants];
    int i = 0;
    foreach_list(constants, ScriptVariant, iter)
    {
        interpreter->constants[i++] = *iter.value();
    }

    // build the globals array
    interpreter->numGlobals = globals.globalVariables.size();
    interpreter->globals = new ScriptVariant[interpreter->numGlobals];
}

ExecFunction *ExecBuilder::getFunctionNamed(const char *name)
{
    if (interpreter->functions.findByName(name))
        return interpreter->functions.retrieve();
    else
        return NULL;
}

static u16 createSrc(RValue *src)
{
    u8 file, index;
    if (src->isTemporary())
    {
        file = FILE_GPR;
        index = static_cast<Temporary*>(src)->reg;
    }
    else if (src->isParam())
    {
        file = FILE_PARAM;
        index = static_cast<Param*>(src)->index;
    }
    else if (src->isGlobalVarRef())
    {
        file = FILE_GLOBAL;
        index = static_cast<GlobalVarRef*>(src)->id;
    }
    else if (src->isConstant())
    {
        int id = static_cast<Constant*>(src)->id;
        file = FILE_CONSTANT + (id / 256);
        index = id % 256;
    }
    return (file << 8) | index;
}

void FunctionBuilder::createExecInstruction(ExecInstruction *inst, Instruction *ssaInst)
{
    inst->opCode = ssaInst->op;
    if (ssaInst->op == OP_CALL)
    {
        // call target
        FunctionCall *ssaCall = static_cast<FunctionCall*>(ssaInst);
        ExecFunction *target = execBuilder->getFunctionNamed(ssaCall->functionRef->functionName);
        assert(target);
        inst->callTarget = nextCallTargetIndex;
        func->callTargets[nextCallTargetIndex++] = target;
        // printf("linked call to %s from %s\n", ssaCall->functionRef->functionName, func->functionName);

        int numParams = ssaInst->operands.size();
        assert(numParams < 256);
        if (numParams > func->maxCallParams)
            func->maxCallParams = numParams;
        inst->paramsIndex = nextParamIndex;
        func->callParams[nextParamIndex++] = numParams;
        foreach_list(ssaInst->operands, RValue, iter)
        {
            func->callParams[nextParamIndex] = createSrc(iter.value());
            ++nextParamIndex;
        }
    }
    else
    {
        int numSrc = ssaInst->operands.size();
        assert(numSrc >= 0 && numSrc <= 2);
        if (numSrc >= 1) // src0
            inst->src0 = createSrc(ssaInst->src(0));
        if (numSrc >= 2) // src1
            inst->src1 = createSrc(ssaInst->src(1));
        if (ssaInst->isJump())
            inst->jumpTarget = static_cast<Jump*>(ssaInst)->target->startIndex;
    }

    // set dst
    if (ssaInst->isExpression())
        inst->dst = static_cast<Expression*>(ssaInst)->value()->reg;
    else if (ssaInst->op == OP_EXPORT)
        inst->dst = static_cast<Export*>(ssaInst)->dst->id;
}

FunctionBuilder::FunctionBuilder(SSABuilder *ssaFunc, ExecBuilder *builder)
    : execBuilder(builder), ssaFunc(ssaFunc), nextParamIndex(0), nextCallTargetIndex(0)
{
    this->func = execBuilder->getFunctionNamed(ssaFunc->functionName);
    assert(func);
}

void FunctionBuilder::run()
{
    func->functionName = strdup(ssaFunc->functionName);
    func->interpreter = execBuilder->interpreter;
    func->numParams = ssaFunc->paramCount;
    func->maxCallParams = 0;

    int numInstructions = 0, numTemps = 0, numCalls = 0, numParams = 0;
    foreach_list(ssaFunc->instructionList, Instruction, iter)
    {
        Instruction *inst = iter.value();
        if (inst->seqIndex < 0) continue;
        ++numInstructions;
        if (inst->op == OP_CALL)
        {
            ++numCalls;
            numParams += inst->operands.size() + 1;
        }
        foreach_list(inst->operands, RValue, srcIter)
        {
            if (srcIter.value()->isTemporary())
            {
                int index = static_cast<Temporary*>(srcIter.value())->reg;
                if (index >= numTemps)
                    numTemps = index + 1;
            }
        }
    }

    func->numInstructions = numInstructions;
    func->instructions = new ExecInstruction[numInstructions];
    memset(func->instructions, 0, numInstructions * sizeof(ExecInstruction));
    func->callTargets = new ExecFunction*[numCalls];
    func->callParams = new u16[numParams];
    func->numGPRs = numTemps;
    foreach_list(ssaFunc->instructionList, Instruction, iter)
    {
        Instruction *inst = iter.value();
        if (inst->seqIndex < 0) continue;
        createExecInstruction(&(func->instructions[inst->seqIndex]), inst);
    }
    printf("built executable function %s\n", func->functionName);
}

static void printSrc(u16 src)
{
    int file = src >> 8, index = src & 0xff;
    if (file == FILE_NONE) return;
    else if (file > FILE_CONSTANT)
        printf("const%i", file - FILE_CONSTANT);
    else
        printf("%s", regFileNames[file]);
    printf("[%i] ", index);
}

void ExecBuilder::printInstructions()
{
    // print constants
    printf("\nConstants: ");
    foreach_list(constants, ScriptVariant, iter)
    {
        ScriptVariant *val = iter.value();
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
            // FIXME: re-escape characters such as '\r' and '\n'
            printf("\"%s\"", StrCache_Get(val->strVal));
        }
        else
        {
            printf("const[?]");
        }
        printf(" ");
    }
    printf("\n");
    // print each function
    foreach_list(interpreter->functions, ExecFunction, funcIter)
    {
        ExecFunction *func = funcIter.value();
        printf("\n~~~ Function '%s' (%i params) ~~~\n", func->functionName, func->numParams);
        for (int i = 0; i < func->numInstructions; i++)
        {
            ExecInstruction *inst = &func->instructions[i];

            // instruction number
            printf("%i: ", i);

            // destination
            if (inst->opCode >= OP_MOV && inst->opCode <= OP_CALL)
            {
                printf("gpr[%i] := ", inst->dst);
            }
            else if (inst->opCode == OP_EXPORT)
            {
                printf("global[%i] := ", inst->dst);
            }

            // opcode
            printf("%s ", opCodeNames[inst->opCode]);

            // parameters/sources
            if (inst->opCode == OP_CALL)
            {
                ExecFunction *callTarget = func->callTargets[inst->callTarget];
                printf("%s ", callTarget->functionName);
                u16 paramCount16 = func->callParams[inst->paramsIndex];
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
