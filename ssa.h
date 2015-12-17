#ifndef SSA_H
#define SSA_H

#include "ralloc.h"
#include "List.h"
#include "Stack.h"
#include "ScriptVariant.h"
#include "SymbolTable.h"

#ifndef __cplusplus
#error C++ header included from C source file
#endif

enum OpCode
{
    OP_NOOP,
    OP_BB_START,
    OP_PHI,
    
    // jumps
    OP_JMP,
    OP_BRANCH_FALSE,
    OP_BRANCH_TRUE,
    OP_RETURN,

    // move
    OP_MOV,

    // unary ops
    OP_NEG,
    OP_BOOL_NOT,
    OP_BIT_NOT,
    
    // binary ops
    OP_BOOL_OR,
    OP_BOOL_AND,
    OP_BIT_OR,
    OP_XOR,
    OP_BIT_AND,
    OP_EQ,
    OP_NE,
    OP_LT,
    OP_GT,
    OP_GE,
    OP_LE,
    OP_SHL,
    OP_SHR,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,

    // function call
    OP_CALL,

    // error
    OP_ERR,
};

static const char *opCodeNames[] = {
    "noop",
    "bb_start",
    "phi",

    "jmp",
    "branch_false",
    "branch_true",
    "return",
    
    "mov",

    "neg",
    "bool_not",
    "bit_not",

    "bool_or",
    "bool_and",
    "bit_or",
    "xor",
    "bit_and",
    "eq",
    "ne",
    "lt",
    "gt",
    "ge",
    "le",
    "shl",
    "shr",
    "add",
    "sub",
    "mul",
    "div",
    "mod",

    "call",
};

class Instruction;
class BasicBlock;

// abstract base class for SSA values
class RValue
{
    DECLARE_RALLOC_CXX_OPERATORS(RValue);
public:
    CList<Instruction> users; // instructions that use this value
    const char *lvalue; // variable name if this is an lvalue, otherwise NULL

    inline RValue() : lvalue(NULL) {}

    // replace all uses of this value with another
    void replaceBy(RValue *newValue);
    
    // referenced by another instruction
    void ref(Instruction *inst);

    // unreferenced by other instruction
    void unref(Instruction *inst);

    virtual bool isConstant();
    virtual bool isTemporary();
    virtual bool isUndefined();
    virtual void printDst() = 0;
};

// an undefined value; will cause a compile error if program depends on it
class Undef : public RValue
{
public:
    inline Undef() : RValue() {}
    virtual bool isUndefined();
    virtual void printDst();
};

// a temporary, i.e. the result of an expression
class Temporary : public RValue
{
public:
    int id;
    inline Temporary(int id) : RValue(), id(id) {}
    virtual bool isTemporary();
    virtual void printDst();
};

// an immediate value
class Constant : public RValue
{
public:
    ScriptVariant constValue;
    Constant(ScriptVariant val);
    Constant(LONG intVal);
    // Constant(double dblVal);
    // Constant(const char *strVal);
    virtual bool isConstant();
    virtual void printDst();
};

// a parameter (input value) to the function
class Param : public RValue
{
public:
    int index;
    inline Param(int index) : index(index) {}
    virtual void printDst();
};

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
    void printOperands();
};

class Expression : public Instruction // subclasses Phi, Constant, Expression, etc.
{
public:
    RValue *dst;

    Expression(OpCode opCode, int valueId, RValue *src0 = NULL, RValue *src1 = NULL);
    inline RValue *value() { return dst; }
    virtual void print();
};

class NoOp : public Instruction
{
public:
    inline NoOp(OpCode opCode = OP_NOOP) : Instruction(opCode) {}
};

class BlockDecl : public NoOp
{
public:
    BasicBlock *block;
    inline BlockDecl(BasicBlock *block) : NoOp(OP_BB_START), block(block) {}
    void print();
};

class Phi : public Expression
{
public:
    inline Phi(int valueId) : Expression(OP_PHI, valueId) {}
};

class FunctionCall : public Expression
{
public:
    char *functionName;
    FunctionCall(const char *functionName, int valueId);
    virtual void print();
};

class Jump : public Instruction
{
public:
    BasicBlock *target;
    Jump(OpCode opCode, BasicBlock *target, RValue *condition);
    void print();
};

class BasicBlock
{
    DECLARE_RALLOC_CXX_OPERATORS(BasicBlock);
public:
    int id;
    bool isSealed;
    CList<BasicBlock> preds; // predecessors
    CList<Phi> incompletePhis;
    
    // these point to nodes in SSABuilder->instructionList
    Node *start;
    Node *end;
    
    inline BasicBlock(int id) : id(id), isSealed(false), start(NULL), end(NULL)
    {}
    
    void addPred(BasicBlock *newPred);
    bool endsWithJump(); // returns true if this block ends with an unconditional jump
    inline bool isEmpty() { return start->next == end; }
    void printName();
    void print();
};

class SSABuilder;
class Loop
{
public:
    BasicBlock *loopEntry, *loopHeader, *loopExit, *bodyEntry, *bodyExit;
    
    Loop(SSABuilder *bld, BasicBlock *predecessor);
};

class SSABuilder // SSA form of a script function, rename to SSAFunction?
{
    DECLARE_RALLOC_CXX_OPERATORS(SSABuilder);
private:
    CList<Instruction> instructionList;
    CList<BasicBlock> basicBlockList;
    CList<Constant> constantList;
    int nextBBId; // init to 0
    int nextValueId; // init to 0
    char identBuf[300]; // buffer for getIdentString result
    CList<RValue> currentDef; // current definitions in each basic block, indexed by "var:block"
    char *functionName; // name of this function

public:
    void *memCtx;
    int paramCount;

    // constructor
    // TODO (void *memCtx, char *name)
    inline SSABuilder(void *memCtx, const char *name = NULL)
        : memCtx(memCtx), nextBBId(0), nextValueId(0), paramCount(0)
    {
        functionName = name ? ralloc_strdup(memCtx, name) : NULL;
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
    // insert instruction at start of basic block
    void insertInstructionAtStart(Instruction *inst, BasicBlock *bb);

    // add a constant
    Constant *addConstant(ScriptVariant sv);
    
    void printInstructionList();
    inline int valueId() { return nextValueId++; }
};

class SSABuildUtil
{
private:
    SSABuilder *builder;
    StackedSymbolTable symbolTable;

    Constant *applyOp(OpCode op, ScriptVariant *src0, ScriptVariant *src1); // used for constant folding
public:
    BasicBlock *currentBlock;
    CStack<BasicBlock> breakTargets;
    CStack<BasicBlock> continueTargets;
    
    SSABuildUtil(SSABuilder *builder);
    ~SSABuildUtil();
    inline void setCurrentBlock(BasicBlock *block) { currentBlock = block; }
    
    // declare a parameter to this function
    void addParam(const char *name);
    
    // these make an instruction, insert it at the end of the current block, and return it
    RValue *mkUnaryOp(OpCode op, RValue *src);
    RValue *mkBinaryOp(OpCode op, RValue *src0, RValue *src1);
    Constant *mkConstInt(LONG val);
    Constant *mkConstString(char *val);
    Constant *mkConstFloat(double val);
    Constant *mkNull();
    Jump *mkJump(OpCode op, BasicBlock *target, RValue *condition);
    Instruction *mkReturn(RValue *src0);
    RValue *mkMove(RValue *val);
    
    // creates a function call instruction but doesn't put it in the instruction list yet
    FunctionCall *startFunctionCall(const char *name);
    // inserts a finished function call instruction at the end of the current block
    void insertFunctionCall(FunctionCall *call);

    Undef *undef(); // get an undefined value

    void declareVariable(const char *name);
    bool writeVariable(const char *variable, RValue *value);
    RValue *readVariable(const char *variable);

    void pushScope();
    void popScope();
};

#endif // !defined(SSA_H)