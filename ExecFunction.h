#include "depends.h"
#include "List.h"
#include "ScriptVariant.h"

enum RegFile {
    FILE_NONE,
    FILE_GPR,
    FILE_PARAM,
    FILE_GLOBAL,
    FILE_IMMEDIATE,
    FILE_CONSTANT,
    // greater numbers are additional constant files
};

static const char *regFileNames[] = {
    "(NONE)",
    "gpr",
    "param",
    "global",
    "imm",
    "const",
};

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
    u8 dst; // index of GPR to store result in (or global if opCode == OP_EXPORT)
    union {
        u16 src0; // first src
        u16 paramsIndex; // for functions
    };
    union {
        u16 src1; // second src
        u16 callTarget; // for functions: index in callTargets (CALL) or builtins (CALL_BUILTIN)
    };
    u16 jumpTarget; // for jumps
};

struct ExecFunction {
    char *functionName;
    Interpreter *interpreter;
    int numParams;
    ExecFunction **callTargets;
    u16 *callParams; // each "param" is actually 8 bits of src file and 8 bits of src index
    int maxCallParams; // largest number of parameters to a single call in this function
    int numInstructions;
    ExecInstruction *instructions;
};

class Interpreter {
public:
    CList<ExecFunction> functions;
    int numConstants;
    ScriptVariant *constants;
    int numGlobals;
    ScriptVariant *globals;
};
