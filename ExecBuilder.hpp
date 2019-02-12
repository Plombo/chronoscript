#ifndef EXEC_BUILDER_HPP
#define EXEC_BUILDER_HPP

#include "Interpreter.hpp"
#include "SSABuilder.hpp"

// builds ExecFunction/ExecInstruction from SSA IR
class ExecBuilder {
public:
    List<SSABuilder*> ssaFunctions;
    List<ExecFunction*> execFunctions;
    List<ScriptVariant*> constants;
    GlobalState globals;
    Interpreter *interpreter;
public:
    ExecBuilder(const char *filename);
    void allocateExecFunctions();
    void buildExecutable();
    ExecFunction *getFunctionNamed(const char *name);
    void printInstructions();
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

