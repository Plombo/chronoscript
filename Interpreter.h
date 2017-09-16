#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "depends.h"
#include "List.h"
#include "ScriptVariant.h"

enum RegFile {
    FILE_NONE,
    FILE_TEMP,
    FILE_PARAM,
    FILE_GLOBAL,
    FILE_CONSTANT,
    // greater numbers are additional constant files
};

inline const char *getRegisterFileName(RegFile f)
{
    const char *regFileNames[] = {
        "(NONE)",
        "temp",
        "param",
        "global",
        "const",
    };
    return regFileNames[f];
}

class Interpreter;

/*
Old Instruction: 3 ints, 8 pointers
    44 bytes on 32-bit platforms
    76 bytes on 64-bit platforms

New ExecInstruction:
    8 bytes on all platforms
*/
struct ExecInstruction {
    u8 opCode;
    u8 dst; // index of temporary to store result in (or global if opCode == OP_EXPORT)
    union {
        u16 src0; // first src
        u16 paramsIndex; // for functions
    };
    union {
        u16 src1; // second src
        u16 callTarget; // for functions: index in callTargets (CALL) or builtins (CALL_BUILTIN)
    };
    union {
        u16 src2; // third src
        u16 jumpTarget; // for jumps
    };
};

struct ExecFunction {
    char *functionName;
    Interpreter *interpreter;
    int numParams;
    int numTemps;
    ExecFunction **callTargets;
    u16 *callParams; // each "param" is actually 8 bits of src file and 8 bits of src index
    int maxCallParams; // largest number of parameters to a single call in this function
    int numInstructions;
    ExecInstruction *instructions;

    inline ExecFunction()
        : functionName(NULL),
          interpreter(NULL),
          numParams(0),
          numTemps(0),
          callTargets(NULL),
          callParams(NULL),
          maxCallParams(0),
          numInstructions(0),
          instructions(NULL)
    {}

    // destructor to free all of the above
    ~ExecFunction();
};

class Interpreter {
public:
    List<ExecFunction*> functions;
    int numConstants;
    ScriptVariant *constants;
    int numGlobals;
    ScriptVariant *globals;

    inline Interpreter() : numConstants(0), constants(NULL), numGlobals(0), globals(NULL)
    {}

    // destructor to free all of the above
    ~Interpreter();

    ExecFunction *getFunctionNamed(const char *name);
    HRESULT runFunction(ExecFunction *function, ScriptVariant *params, ScriptVariant *retval);
};


#endif

