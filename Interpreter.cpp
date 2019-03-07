#include <stdio.h>
#include "Interpreter.hpp"
#include "ScriptVariant.hpp"
#include "ScriptObject.hpp"
#include "ObjectHeap.hpp"
#include "Builtins.hpp"
#include "SSABuilder.hpp" // for opcodes

typedef CCResult (*UnaryOperation)(ScriptVariant *dst, const ScriptVariant *src);
typedef CCResult (*BinaryOperation)(ScriptVariant *dst, const ScriptVariant *src1, const ScriptVariant *src2);

// does the actual work of executing the script
static CCResult execFunction(ExecFunction *function, ScriptVariant *params, ScriptVariant *retval)
{
    int index = 0;
    ScriptVariant temps[function->numTemps];
    ScriptVariant callParams[function->maxCallParams];
    ScriptVariant *srcFiles[] = {
        NULL,
        temps,
        params,
        function->interpreter->globals,
        function->interpreter->constants
    };
    UnaryOperation unaryOps[] = {
        ScriptVariant_Neg,
        ScriptVariant_Boolean_Not,
        ScriptVariant_Bit_Not,
        ScriptVariant_ToBoolean
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
    #define fetchDst() dst = &temps[inst->dst];

    while(1)
    {
        ExecInstruction *inst = &function->instructions[index];
        bool jumped = false;
        ScriptVariant *dst, *src0, *src1, *src2, *paramTemp;
        ScriptVariant scratch;
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
                ScriptVariant_Eq(&scratch, src0, src1);
                shouldBranch = ScriptVariant_IsTrue(&scratch);
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
                return CC_OK;

            // move
            case OP_MOV:
            case OP_GET_GLOBAL:
                fetchDst();
                fetchSrc(src0, inst->src0);
                *dst = *src0;
                break;

            // unary ops
            case OP_NEG:
            case OP_BOOL_NOT:
            case OP_BIT_NOT:
            case OP_BOOL:
                fetchDst();
                fetchSrc(src0, inst->src0);
                if (unaryOps[inst->opCode - OP_NEG](dst, src0) == CC_FAIL)
                {
                    printf("error: an exception occurred when executing %s instruction\n",
                        getOpCodeName((OpCode)inst->opCode));
                    goto start_backtrace;
                }
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
                if (binaryOps[inst->opCode - OP_BIT_OR](dst, src0, src1) == CC_FAIL)
                {
                    printf("error: an exception occurred when executing %s instruction\n",
                        getOpCodeName((OpCode)inst->opCode));
                    goto start_backtrace;
                }
                break;

            // function call
            case OP_CALL:
            case OP_CALL_BUILTIN:
            {
                fetchDst();
                int numParams = function->callParams[inst->paramsIndex];
                for (int i = 0; i < numParams; i++)
                {
                    fetchSrc(paramTemp, function->callParams[inst->paramsIndex+i+1]);
                    callParams[i] = *paramTemp;
                }
                CCResult callResult;
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
                if (CC_FAIL == callResult)
                {
                    if (inst->opCode == OP_CALL_BUILTIN)
                    {
                        printf("\n\nan exception occurred in a builtin script function\n"); // TODO: function name
                    }
                    goto continue_backtrace;
                }
                break;
            }

            // operations to create/modify/access objects and lists
            case OP_MKOBJECT:
                fetchDst();
                fetchSrc(src0, inst->src0);
                dst->vt = VT_OBJECT;
                dst->objVal = ObjectHeap_CreateNewObject(src0->lVal);
                break;
            case OP_MKLIST:
                fetchDst();
                fetchSrc(src0, inst->src0);
                dst->vt = VT_LIST;
                dst->objVal = ObjectHeap_CreateNewList((size_t)src0->lVal);
                break;
            case OP_SET:
                fetchSrc(src0, inst->src0);
                fetchSrc(src1, inst->src1);
                fetchSrc(src2, inst->src2);
                if (CC_FAIL == ScriptVariant_ContainerSet(src0, src1, src2))
                {
                    printf("error: SET operation failed\n");
                    goto start_backtrace;
                }
                break;
            case OP_GET:
                fetchDst();
                fetchSrc(src0, inst->src0);
                fetchSrc(src1, inst->src1);
                if (CC_FAIL == ScriptVariant_ContainerGet(dst, src0, src1))
                {
                    printf("error: GET operation failed\n");
                    goto start_backtrace;
                }
                break;

            // write to global variable
            case OP_EXPORT:
                dst = &function->interpreter->globals[inst->dst];
                fetchSrc(src0, inst->src0);
                ScriptVariant_Unref(dst);
                *dst = *src0;
                ScriptVariant_Ref(dst);
                break;
            default:
                printf("error: unknown opcode %i\n", inst->opCode);
                return CC_FAIL;
        }
        if (!jumped) index++;
    }
    return CC_OK;

start_backtrace:
    printf("\n\nAn exception occurred in script function %s() in %s\n",
        function->functionName,
        function->interpreter->fileName);
    return CC_FAIL;

continue_backtrace:
    printf("called from %s() in %s\n",
        function->functionName,
        function->interpreter->fileName);
    return CC_FAIL;
}

ExecFunction *Interpreter::getFunctionNamed(const char *name)
{
    return functions.findByName(name) ? functions.retrieve() : NULL;
}

CCResult Interpreter::runFunction(ExecFunction *function, ScriptVariant *params, ScriptVariant *retval)
{
    CCResult result = execFunction(function, params, retval);
    if (result == CC_OK)
    {
        ScriptVariant_Ref(retval);
    }
    ObjectHeap_ClearTemporary();
    StrCache_ClearTemporary();
    return result;
}

Interpreter::~Interpreter()
{
    // free constants and globals
    for (int i = 0; i < numConstants; i++)
    {
        // clear variant to prevent string cache leaks
        ScriptVariant_Unref(&constants[i]);
    }
    for (int i = 0; i < numGlobals; i++)
    {
        ScriptVariant_Unref(&globals[i]);
    }
    delete[] constants;
    delete[] globals;

    // free functions
    foreach_list(functions, ExecFunction*, iter)
    {
        delete iter.value();
    }
    functions.clear();

    free(fileName);
}

ExecFunction::~ExecFunction()
{
    free(functionName);
    delete[] callTargets;
    delete[] callParams;
    delete[] instructions;
}

