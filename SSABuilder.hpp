#ifndef SSA_BUILDER_HPP
#define SSA_BUILDER_HPP

#include "ralloc.h"
#include "List.hpp"
#include "ScriptVariant.hpp"
#include "SymbolTable.hpp"
#include "RegAllocUtil.hpp"

// be sure to update getOpCodeName() in Instruction.cpp if you change this enum!!
enum OpCode
{
    OP_NOOP,
    OP_BB_START,
    OP_PHI,
    
    // jumps
    OP_JMP,
    OP_BRANCH_FALSE,
    OP_BRANCH_TRUE,
    OP_BRANCH_EQUAL,
    OP_RETURN,

    // move
    OP_MOV,
    OP_GET_GLOBAL,

    // unary ops
    OP_NEG,
    OP_BOOL_NOT,
    OP_BIT_NOT,
    OP_BOOL,
    
    // binary ops
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
    OP_CALL_BUILTIN,

    // object/list operations
    OP_MKOBJECT,
    OP_MKLIST,
    OP_GET,
    OP_SET,

    // write to global variable
    OP_EXPORT,

    // error
    OP_ERR,
};

const char *getOpCodeName(OpCode op);

// RValue and its subclasses
class RValue;
class Temporary;
class Constant;
class GlobalVarRef;
class Param;

// Instruction and its subclasses
class Instruction;
class Expression;
class BlockDecl;
class Phi;
class FunctionCall;
class Jump;
class Export;

class LValue;
class BasicBlock;
class Loop;
class SSABuilder;

// abstract base class for SSA values
class RValue
{
    DECLARE_RALLOC_CXX_OPERATORS(RValue);
public:
    List<Instruction*> users; // instructions that use this value
    LValue *lvalue; // non-NULL means this is an lvalue

    inline RValue() : lvalue(NULL) {}

    // trivial virtual destructor to silence compiler warnings
    virtual ~RValue();

    // returns true if this value is unused
    inline bool isDead() { return users.size() == 0; }

    // replace all uses of this value with another
    void replaceBy(RValue *newValue);
    
    // referenced by another instruction
    void ref(Instruction *inst);

    // unreferenced by other instruction
    void unref(Instruction *inst);

    virtual bool isConstant();
    virtual bool isTemporary();
    virtual bool isParam();
    virtual bool isGlobalVarRef();
    virtual bool isUndefined();
    virtual bool isBoolValue();

    inline Constant *asConstant();
    inline Temporary *asTemporary();
    inline Param *asParam();
    inline GlobalVarRef *asGlobalVarRef();

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
    Expression *expr;
    inline Temporary(int id, Expression *expr) : RValue(), id(id), expr(expr), reg(-1) {}
    virtual bool isTemporary();
    virtual void printDst();
    virtual bool isBoolValue();
    
    // for register allocation
    // Interval livei;
    // Temporary *mcsPrev, *mcsNext;
    // int cardinality;
    int reg; // register number; -1 if RA hasn't happened yet
};

// reference a global variable, defined outside of the function scope
class GlobalVarRef : public RValue
{
public:
    int id; // index of the referenced global variable
    inline GlobalVarRef(int id) : RValue(), id(id) {}
    virtual bool isGlobalVarRef();
    virtual void printDst();
};

// an immediate value
class Constant : public RValue
{
public:
    int id;
    ScriptVariant constValue;
    Constant(ScriptVariant val);
    Constant(int32_t intVal);
    // Constant(double dblVal);
    // Constant(const char *strVal);
    virtual bool isConstant();
    virtual void printDst();
    virtual bool isBoolValue();
};

// a parameter (input value) to the function
class Param : public RValue
{
public:
    int index;
    inline Param(int index) : index(index) {}
    virtual bool isParam();
    virtual void printDst();
};

Constant *RValue::asConstant() { return static_cast<Constant*>(this); }
Temporary *RValue::asTemporary() { return static_cast<Temporary*>(this); }
Param *RValue::asParam() { return static_cast<Param*>(this); }
GlobalVarRef *RValue::asGlobalVarRef() { return static_cast<GlobalVarRef*>(this); }

class LValue
{
    DECLARE_RALLOC_CXX_OPERATORS(LValue);
public:
    // this is non-NULL for lvalues read from a variable
    const char *varName;

    // these are non-NULL for lvalues defined by a GET instruction
    RValue *parent; // parent object/array if this is an lvalue referring to an object member
    RValue *key;

    inline LValue(const char *varName)
        : varName(ralloc_strdup(this, varName)), parent(NULL), key(NULL) {}

    inline LValue(RValue *parent, RValue *key)
        : varName(NULL), parent(parent), key(key) {}
};

class Instruction
{
    DECLARE_RALLOC_CXX_OPERATORS(Instruction);
public:
    OpCode op; // opcode
    List<RValue*> operands; // sources
    BasicBlock *block; // basic block
    int seqIndex; // for live ranges in register allocation
    bool isPhiMove; // this instruction is a move used by a phi in a successor block

    inline Instruction(OpCode opCode) : op(opCode), block(NULL), seqIndex(-1), isPhiMove(false) {}

    // trivial virtual destructor to silence compiler warnings
    virtual ~Instruction();

    void appendOperand(RValue *value);
    RValue *src(int index);
    void setSrc(int index, RValue *val);
    
    virtual void print();
    void printOperands();
    virtual bool isExpression();
    virtual bool isJump();
    inline bool isPhi() { return op == OP_PHI; }
    inline bool isFunctionCall() { return op == OP_CALL || op == OP_CALL_BUILTIN; }

    inline Expression *asExpression();
    inline Phi *asPhi();
    inline FunctionCall *asFunctionCall();
    inline Jump *asJump();
    inline Export *asExport();

    // used after register allocation
    virtual bool isTrivial();
};

class Expression : public Instruction // subclasses Phi, Constant, Expression, etc.
{
public:
    Temporary *dst;

    Expression(OpCode opCode, int valueId, RValue *src0 = NULL, RValue *src1 = NULL);
    inline Temporary *value() { return dst; }
    virtual void print();
    virtual bool isExpression();
    virtual bool isTrivial();
    virtual bool isDead();
};

class NoOp : public Instruction
{
public:
    inline NoOp(OpCode opCode = OP_NOOP) : Instruction(opCode) {}
    virtual bool isTrivial();
};

class BlockDecl : public NoOp
{
public:
    //BasicBlock *block;
    inline BlockDecl(BasicBlock *block) : NoOp(OP_BB_START) { this->block = block; }
    void print();
};

class Phi : public Expression
{
public:
    inline Phi(int valueId) : Expression(OP_PHI, valueId) {}
    virtual bool isTrivial();
    BasicBlock **sourceBlocks;
};

struct ExecFunction;
class FunctionCall : public Expression
{
public:
    char *functionName;
    union { // this value set during linking
        ExecFunction *functionRef; // op == OP_CALL
        int builtinRef; // op == OP_CALL_BUILTIN
    };
    FunctionCall(const char *functionName, int valueId);
    virtual bool isDead();
    virtual void print();
};

class Jump : public Instruction
{
public:
    BasicBlock *target;
    Jump(OpCode opCode, BasicBlock *target, RValue *src0, RValue *src1);
    void print();
    bool isJump();
};

class Export : public Instruction
{
public:
    GlobalVarRef *dst;
    inline Export(GlobalVarRef *dst, RValue *src) : Instruction(OP_EXPORT), dst(dst)
    {
        appendOperand(src);
    }
    void print();
};

Expression *Instruction::asExpression() { return static_cast<Expression*>(this); }
Phi *Instruction::asPhi() { return static_cast<Phi*>(this); }
FunctionCall *Instruction::asFunctionCall() { return static_cast<FunctionCall*>(this); }
Jump *Instruction::asJump() { return static_cast<Jump*>(this); }
Export *Instruction::asExport() { return static_cast<Export*>(this); }

class BasicBlock
{
    DECLARE_RALLOC_CXX_OPERATORS(BasicBlock);
public:
    int id;
    bool isSealed;
    bool hasAssignment; // an assignment is done in this basic block (so it is not empty)
    List<BasicBlock*> preds; // predecessors
    List<Phi*> incompletePhis;
    
    // these point to nodes in SSABuilder->instructionList
    Node<Instruction*> *start;
    Node<Instruction*> *end;
    
    // loop that this basic block is part of
    Loop *loop;
    
    // for liveness analysis
    List<BasicBlock*> succs; // successors
    bool processed; // processed by depth-first search
    BitSet liveIn;
    BitSet liveOut;
    BitSet phiDefs;
    BitSet phiUses;

    // index in final instruction list where this block starts
    int startIndex;

    inline BasicBlock(int id)
       : id(id), isSealed(false), hasAssignment(false), start(NULL),
         end(NULL), loop(NULL), startIndex(-1)
    {}

    void addPred(BasicBlock *newPred);
    bool endsWithJump(); // returns true if this block ends with an unconditional jump
    inline bool isEmpty() { return start->next == end && !hasAssignment; }
    void printName();
    void print();

    // true if this dominates b, and the path from this to b doesn't include 'without'
    bool dominates(BasicBlock *b, BasicBlock *without, int numBlocks);
private:
    bool dominates(BasicBlock *b, BasicBlock *without, BitSet *tested);
};

class Loop
{
    DECLARE_RALLOC_CXX_OPERATORS(Loop);
public:
    BasicBlock *header;
    List<BasicBlock*> nodes;
    Loop *parent;
    List<Loop*> children;
    
    Loop(BasicBlock *header, Loop *parent = NULL);
};

class SSABuilder // SSA form of a script function
{
    DECLARE_RALLOC_CXX_OPERATORS(SSABuilder);
public:
    List<Instruction*> instructionList;
    List<BasicBlock*> basicBlockList;
    List<Loop*> loops; // loop-nesting forest
    List<Temporary*> temporaries; // constructed right before regalloc
    List<Constant*> constantList;
    char *functionName; // name of this function
    void *memCtx;
private:
    int nextBBId; // init to 0
    int nextValueId; // init to 0
    char identBuf[300]; // buffer for getIdentString result
    List<RValue*> currentDef; // current definitions in each basic block, indexed by "var:block"

public:
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
    
    // dead code elimination
    bool removeDeadCode();
    void prepareForRegAlloc();
    
    void printInstructionList();
    inline int valueId() { return nextValueId++; }
};

// state for global variables, enums, and anything else not function-local
class GlobalState
{
    friend class ExecBuilder;
private:
    // name=var name, value=initial value
    List<ScriptVariant> globalVariables;
public:
    inline GlobalState() {}
    inline ~GlobalState() {}
    void declareGlobalVariable(const char *varName, ScriptVariant initialValue);
    // bool writeGlobalVariable(const char *variable, RValue *value);
    GlobalVarRef *readGlobalVariable(const char *varName, void *memCtx);

    inline bool globalVariableExists(const char *varName)
    {
        return globalVariables.findByName(varName);
    }
};

class SSABuildUtil
{
    DECLARE_RALLOC_CXX_OPERATORS(SSABuildUtil);
private:
    SSABuilder *builder;
    StackedSymbolTable symbolTable;
    Loop *currentLoop;
    GlobalState *globalState;

    Constant *applyOp(OpCode op, ScriptVariant *src0, ScriptVariant *src1); // used for constant folding
public:
    BasicBlock *currentBlock;
    Stack<BasicBlock*> breakTargets;
    Stack<BasicBlock*> continueTargets;
    
    SSABuildUtil(SSABuilder *builder, GlobalState *globalState);
    inline void setCurrentBlock(BasicBlock *block) { currentBlock = block; }
    
    // declare a parameter to this function
    void addParam(const char *name);
    
    // these make an instruction, insert it at the end of the current block, and return it
    RValue *mkUnaryOp(OpCode op, RValue *src);
    RValue *mkBinaryOp(OpCode op, RValue *src0, RValue *src1);
    Constant *mkConstInt(int32_t val);
    Constant *mkConstString(char *val);
    Constant *mkConstFloat(double val);
    Constant *mkNull();
    Jump *mkJump(OpCode op, BasicBlock *target, RValue *src0, RValue *src1 = NULL);
    Instruction *mkReturn(RValue *src0);
    RValue *mkBool(RValue *src);
    RValue *mkMove(RValue *val);
    RValue *mkGetGlobal(RValue *val);
    RValue *mkObject();
    RValue *mkList();
    RValue *mkGet(RValue *object, RValue *key);
    Instruction *mkSet(RValue *object, RValue *key, RValue *value);
    Export *mkExport(GlobalVarRef *dst, RValue *src);
    
    // creates a function call instruction but doesn't put it in the instruction list yet
    FunctionCall *startFunctionCall(const char *name);
    // inserts a finished function call instruction at the end of the current block
    void insertFunctionCall(FunctionCall *call);

    Undef *undef(); // get an undefined value

    bool declareVariable(const char *name);
    bool writeVariable(const char *variable, RValue *value);
    bool mkAssignment(LValue *lhs, RValue *rhs);
    RValue *readVariable(const char *variable);

    void pushScope();
    void popScope();
    
    void pushLoop(Loop *loop);
    void popLoop();
    
    // create BB and insert after the given block
    BasicBlock *createBBAfter(BasicBlock *existingBB, Loop *loop = NULL);
};

#endif // !defined(SSA_BUILDER_HPP)

