#include <stdio.h>
#include "ExecFunction.h"
#include "ScriptVariant.h"
#include "Builtins.h"
#include "ssa.h" // for opcodes

typedef ScriptVariant *(*UnaryOperation)(ScriptVariant*);
typedef ScriptVariant *(*BinaryOperation)(ScriptVariant*, ScriptVariant*);

HRESULT Interpreter::runFunction(ExecFunction *function, ScriptVariant *params, ScriptVariant *retval)
{
    StrCache_SetExecuting(true);
    HRESULT result = execFunction(function, params, retval);
    if (result == S_OK && retval->vt == VT_STR)
        retval->strVal = StrCache_MakePersistent(retval->strVal);
    StrCache_SetExecuting(false);
    return result;
}

HRESULT execFunction(ExecFunction *function, ScriptVariant *params, ScriptVariant *retval)
{
    int index = 0;
    ScriptVariant gprs[function->numGPRs];
    ScriptVariant callParams[function->maxCallParams];
    ScriptVariant *srcFiles[] = {
        NULL,
        gprs,
        params,
        function->interpreter->globals,
        function->interpreter->constants
    };
    UnaryOperation unaryOps[] = {
        ScriptVariant_Neg,
        ScriptVariant_Boolean_Not,
        ScriptVariant_Bit_Not
    };
    BinaryOperation binaryOps[] = {
        ScriptVariant_Bit_Or,
        ScriptVariant_Xor,
        ScriptVariant_Bit_And,
        ScriptVariant_Eq,
        ScriptVariant_Ne,
        ScriptVariant_Lt,
        ScriptVariant_Gt,
        ScriptVariant_Ge,
        ScriptVariant_Le,
        ScriptVariant_Shl,
        ScriptVariant_Shr,
        ScriptVariant_Add,
        ScriptVariant_Sub,
        ScriptVariant_Mul,
        ScriptVariant_Div,
        ScriptVariant_Mod,
    };
    
    #define fetchSrc(dst, src) { \
        int regFile = src >> 8;\
        int regIndex = src & 0xff;\
            if (regFile > FILE_CONSTANT)\
            {\
                regIndex += 256 * (regFile - FILE_CONSTANT);\
                regFile = FILE_CONSTANT;\
            }\
            dst = &srcFiles[regFile][regIndex];\
        }
    #define fetchDst() dst = &gprs[inst->dst];

    while(1)
    {
        ExecInstruction *inst = &function->instructions[index];
        bool jumped = false;
        ScriptVariant *dst, *src0, *src1, *scratch;
        bool shouldBranch;
        switch(inst->opCode)
        {
            // jumps
            case OP_JMP:
                index = inst->jumpTarget;
                jumped = true;
                break;
            case OP_BRANCH_FALSE:
            case OP_BRANCH_TRUE:
                fetchSrc(src0, inst->src0);
                shouldBranch = ScriptVariant_IsTrue(src0);
                if (inst->opCode == OP_BRANCH_FALSE)
                    shouldBranch = !shouldBranch;
                if (shouldBranch)
                {
                    index = inst->jumpTarget;
                    jumped = true;
                }
                break;
            case OP_BRANCH_EQUAL:
                fetchSrc(src0, inst->src0);
                fetchSrc(src1, inst->src1);
                scratch = ScriptVariant_Eq(src0, src1);
                shouldBranch = ScriptVariant_IsTrue(scratch);
                if (shouldBranch)
                {
                    index = inst->jumpTarget;
                    jumped = true;
                }
                break;

            // return
            case OP_RETURN:
                if (inst->src0)
                {
                    fetchSrc(src0, inst->src0);
                    *retval = *src0;
                }
                else
                {
                    retval->vt = VT_EMPTY;
                    retval->ptrVal = NULL;
                }
                return S_OK;

            // move
            case OP_MOV:
                fetchDst();
                fetchSrc(src0, inst->src0);
                *dst = *src0;
                break;

            // unary ops
            case OP_NEG:
            case OP_BOOL_NOT:
            case OP_BIT_NOT:
                fetchDst();
                fetchSrc(src0, inst->src0);
                *dst = *(unaryOps[inst->opCode - OP_NEG](src0));
                break;

            // binary ops
            case OP_BIT_OR:
            case OP_XOR:
            case OP_BIT_AND:
            case OP_EQ:
            case OP_NE:
            case OP_LT:
            case OP_GT:
            case OP_GE:
            case OP_LE:
            case OP_SHL:
            case OP_SHR:
            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
            case OP_DIV:
            case OP_MOD:
                fetchDst();
                fetchSrc(src0, inst->src0);
                fetchSrc(src1, inst->src1);
                *dst = *(binaryOps[inst->opCode - OP_BIT_OR](src0, src1));
                break;

            // function call
            case OP_CALL:
            case OP_CALL_BUILTIN:
            {
                fetchDst();
                int numParams = function->callParams[inst->paramsIndex];
                for (int i = 0; i < numParams; i++)
                {
                    fetchSrc(scratch, function->callParams[inst->paramsIndex+i+1]);
                    callParams[i] = *scratch;
                }
                HRESULT callResult;
                if (inst->opCode == OP_CALL_BUILTIN)
                {
                    BuiltinScriptFunction target = getBuiltinByIndex(inst->callTarget);
                    callResult = target(numParams, callParams, dst);
                }
                else // inst->opCode == OP_CALL
                {
                    ExecFunction *target = function->callTargets[inst->callTarget];
                    callResult = execFunction(target, callParams, dst);
                }
                if (FAILED(callResult))
                    return callResult;
                break;
            }

            // write to global variable
            case OP_EXPORT:
                dst = &function->interpreter->globals[inst->dst];
                fetchSrc(src0, inst->src0);
                *dst = *src0;
                break;
            default:
                printf("error: unknown opcode %i\n", inst->opCode);
                return E_FAIL;
        }
        if (!jumped) index++;
    }
    return S_OK;
}
