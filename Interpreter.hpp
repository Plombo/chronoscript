#ifndef INTERPRETER_HPP
#define INTERPRETER_HPP

#include "depends.h"
#include "List.hpp"
#include "ScriptVariant.hpp"

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
    uint8_t opCode;
    uint8_t dst; // index of temporary to store result in (or global if opCode == OP_EXPORT)
    union {
        uint16_t src0; // first src
        uint16_t paramsIndex; // for functions
    };
    union {
        uint16_t src1; // second src
        uint16_t callTarget; // for functions: index in callTargets (CALL) or builtins (CALL_BUILTIN)
    };
    union {
        uint16_t src2; // third src
        uint16_t jumpTarget; // for jumps
    };
};

struct ExecFunction {
    char *functionName;
    Interpreter *interpreter;
    int numParams;
    int numTemps;
    ExecFunction **callTargets;
    uint16_t *callParams; // each "param" is actually 8 bits of src file and 8 bits of src index
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
    char *fileName;
    List<ExecFunction*> functions;
    int numConstants;
    ScriptVariant *constants;
    int numGlobals;
    ScriptVariant *globals;

    inline Interpreter(const char *filePath) :
        numConstants(0), constants(NULL), numGlobals(0), globals(NULL)
    {
        this->fileName = strdup(filePath);
    }

    // destructor to free all of the above
    ~Interpreter();

    ExecFunction *getFunctionNamed(const char *name);
    HRESULT runFunction(ExecFunction *function, ScriptVariant *params, ScriptVariant *retval);
};


#endif

