#ifndef EXEC_BUILDER_H
#define EXEC_BUILDER_H

#include "ExecFunction.h"
#include "ssa.h"

// builds ExecFunction/ExecInstruction from SSA IR
class ExecBuilder {
    CList<SSABuilder> *ssaFunctions;
    CList<ExecFunction> execFunctions;
public:
    ExecBuilder(CList<SSABuilder> *ssaFunctions);
    ExecFunction *getFunctionNamed(const char *name);
    void printInstructions(); // XXX: move to Interpreter
};

class FunctionBuilder {
private:
    ExecBuilder *execBuilder;
    SSABuilder *ssaFunc;
    ExecFunction *func;
    int nextParamIndex;
    int nextCallTargetIndex;

    void createExecInstruction(ExecInstruction *inst, Instruction *ssaInst);
public:
    FunctionBuilder(SSABuilder *ssaFunc, ExecBuilder *builder);
    void run();
};

#endif
