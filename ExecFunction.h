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
        struct {
            u8 src0; // src0 index
            u8 src1; // src1 index
        };
        u16 paramsIndex; // for functions
    };
    union {
        struct {
            u8 src0File;
            u8 src1File;
        };
        u16 callTarget; // for functions: index in callTargets (CALL) or builtins (CALL_BUILTIN)
    };
    u16 jumpTarget; // for jumps
};

struct ExecFunction {
    char *functionName;
    int numParams;
    ScriptVariant *constants; // TODO move constants to Interpreter
    int numConstants;
    ExecFunction **callTargets;
    u16 *callParams; // each "param" is actually 8 bits of src file and 8 bits of src index
    int numInstructions;
    ExecInstruction *instructions;
};
