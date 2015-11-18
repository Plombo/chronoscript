#ifndef SSA_H
#define SSA_H

#include "ralloc.h"
#include "List.h"
#include "ScriptVariant.h"

#ifndef __cplusplus
#error C++ header included from C source file
#endif

enum OpCode
{
    OP_NOOP,
    OP_PHI,

    OP_CONST,
    OP_UNDEF,

    // unary ops
    OP_NEG,
    OP_BOOL_NOT,
    OP_BIT_NOT,

    // function call
    OP_CALL,
};

static const char *opCodeNames[] = {
    "noop",
    "phi",

    "const",
    "undef",

    "neg",
    "bool_not",
    "bit_not",

    "call",
};

class RValue;

class Instruction
{
    DECLARE_RALLOC_CXX_OPERATORS(Instruction);
public:
    OpCode op; // opcode
    CList<RValue> operands; // sources

    inline Instruction(OpCode opCode) : op(opCode) {}

    void appendOperand(RValue *value);
    RValue *src(int index);
    void setSrc(int index, RValue *val);
    
    virtual void print();
};

class RValue : public Instruction // subclasses Phi, Constant, Expression, etc.
{
public:
    CList<Instruction> users; // instructions that use this value

    RValue(OpCode opCode, RValue *src0 = NULL, RValue *src1 = NULL);

    // replace all uses of this value with another
    void replaceBy(RValue *newValue);
    
    // referenced by another instruction
    void ref(Instruction *inst);

    // unreferenced by other instruction
    void unref(Instruction *inst);

    virtual bool isConstant();
    virtual void print();
    void printDst();
};

class NoOp : public Instruction
{
public:
    inline NoOp(OpCode opCode = OP_NOOP) : Instruction(opCode) {}
};

class Constant : public RValue
{
public:
    ScriptVariant constValue;
    Constant(ScriptVariant val);
    Constant(LONG intVal);
    // Constant(double dblVal);
    // Constant(const char *strVal);
    virtual bool isConstant();
    virtual void print();
};

class Phi : public RValue
{
public:
    inline Phi() : RValue(OP_PHI) {}
};

class Undef : public RValue
{
public:
    inline Undef() : RValue(OP_UNDEF) {}
};

class BasicBlock
{
    DECLARE_RALLOC_CXX_OPERATORS(BasicBlock);
public:
    int id;
    bool isSealed;
    CList<BasicBlock> preds; // predecessors
    CList<Phi> incompletePhis;
    
    // nodes in SSABuilder->instructionList
    Node *start;
    Node *end;
    
    inline BasicBlock(int id) : id(id), isSealed(false), start(NULL), end(NULL)
    {}
    
    void addPred(BasicBlock *newPred);
};

class SSABuilder;
class Loop
{
public:
    BasicBlock *loopEntry, *loopHeader, *loopExit, *bodyEntry, *bodyExit;
    
    Loop(SSABuilder *bld, BasicBlock *predecessor);
};

class SSABuilder // rename to SSAProgram?
{
    DECLARE_RALLOC_CXX_OPERATORS(SSABuilder);
private:
    CList<Instruction> instructionList;
    CList<BasicBlock> basicBlockList;
    int nextBBId; // init to 0
    char identBuf[300]; // buffer for getIdentString result
    CList<RValue> currentDef; // current definitions in each basic block, indexed by "var:block"

public:
    void *memCtx; // = ralloc_context(NULL)

    // constructor
    inline SSABuilder() : memCtx(ralloc_context(NULL)), nextBBId(0)
    {
    }

    // destructor
    inline ~SSABuilder()
    {
        ralloc_free(memCtx);
    }

    // get string used as name in currentDef (e.g., "foo:3" for var "foo" in BB 3)
    const char *getIdentString(const char *variable, BasicBlock *block);

    // ---- from paper ----
    void writeVariable(const char *variable, BasicBlock *block, RValue *value);
    RValue *readVariable(const char *variable, BasicBlock *block);
    RValue *readVariableRecursive(const char *variable, BasicBlock *block);
    RValue *addPhiOperands(const char *variable, BasicBlock *block, Phi *phi);
    RValue *tryRemoveTrivialPhi(Phi *phi);
    void sealBlock(BasicBlock *block);

    // create BB and insert after the given block
    BasicBlock *createBBAfter(BasicBlock *existingBB);

    // insert instruction at end of basic block
    void insertInstruction(Instruction *inst, BasicBlock *bb);
    
    void printInstructionList();
};

class SSABuildUtil
{
private:
    SSABuilder *builder;
    Constant *applyOp(OpCode op, Constant *src0, Constant *src1); // used for constant folding
public:
    inline SSABuildUtil(SSABuilder *builder) : builder(builder) {}
    RValue *mkUnaryOp(OpCode op, RValue *src, BasicBlock *bb);
    RValue *mkBinaryOp(OpCode op, RValue *src0, RValue *src1, BasicBlock *bb);
};

#endif // !defined(SSA_H)
