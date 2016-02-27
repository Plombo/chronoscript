#include <stdio.h>
#include "pp_parser.h"
#include "Parser.h"
#include "List.h"
#include "regalloc.h"
#include "liveness.h"
#include "ExecBuilder.h"
#include "Builtins.h"

void compile(SSABuilder *func)
{
    printf("\n~~~~~ %s ~~~~~\n", func->functionName);
#if 0
    printf("Instructions before processing:\n");
    func->printInstructionList();
#endif
    while(func->removeDeadCode());
    func->prepareForRegAlloc();
    printf("\nInstructions after processing:\n");
    func->printInstructionList();
    printf("\n%i temporaries\n\n", func->temporaries.size());

    computeLiveSets(func);
    printf("\nLive sets:\n");
    foreach_list(func->basicBlockList, BasicBlock, iter)
    {
        BasicBlock *block = iter.value();
        printf("BB %i: in: ", block->id);
        block->liveIn.print();
        printf(", out:", block->id);
        block->liveOut.print();
        printf("\n");
    }
    
    LivenessAnalyzer livenessAnalyzer(func);
    livenessAnalyzer.computeLiveIntervals();
    livenessAnalyzer.coalesce();
    livenessAnalyzer.buildInterferenceGraph();
    
    RegAlloc registerAllocator(livenessAnalyzer.values, livenessAnalyzer.uniqueNodes);
    registerAllocator.run();
    for (int i = 0; i < livenessAnalyzer.uniqueNodes; i++)
    {
        printf("node %i -> $%i\n", livenessAnalyzer.values[i]->id, livenessAnalyzer.values[i]->color);
    }
    
    // assign registers
    foreach_list(func->temporaries, Temporary, iter)
    {
        Temporary *value = iter.value();
        value->reg = livenessAnalyzer.nodeForTemp[value->id]->color;
    }

    // print instruction list again
    printf("\n");
    func->printInstructionList();
    printf("\n");

    // give instructions their final indices
    int nextIndex = 0;
    foreach_list(func->instructionList, Instruction, iter)
    {
        Instruction *inst = iter.value();
        if (inst->op == OP_BB_START)
            inst->block->startIndex = nextIndex;
        if (inst->isTrivial()) inst->seqIndex = -1;
        else
        {
            inst->seqIndex = nextIndex++;
        }
    }
    // print the final instruction list
    foreach_list(func->instructionList, Instruction, iter)
    {
        Instruction *inst = iter.value();
        if (inst->seqIndex >= 0) inst->print();
    }
}

// this is temporary; the final linking process will have to handle
// imports and builtins
void link(SSABuilder *func, CList<SSABuilder> *allFunctions)
{
    foreach_list(func->instructionList, Instruction, iter)
    {
        Instruction *inst = iter.value();
        if (inst->op != OP_CALL) continue;
        FunctionCall *call = static_cast<FunctionCall*>(inst);
        if (allFunctions->findByName(call->functionName))
        {
            call->functionRef = allFunctions->retrieve();
            printf("linked call to %s\n", call->functionName);
        }
        else
        {
            int builtin = getBuiltinIndex(call->functionName);
            if (builtin >= 0)
            {
                call->op = OP_CALL_BUILTIN;
                call->builtinRef = builtin;
                printf("linked call to builtin %s\n", call->functionName);
            }
            else
                printf("error: couldn't link %s\n", call->functionName);
        }
    }
}

void linkConstants(SSABuilder *func, CList<ScriptVariant> *constants)
{
    foreach_list(func->instructionList, Instruction, instIter)
    {
        Instruction *inst = instIter.value();
        foreach_list(inst->operands, RValue, srcIter)
        {
            if (!srcIter.value()->isConstant()) continue;
            Constant *c = static_cast<Constant*>(srcIter.value());
            int i = 0;
            // try to use an existing constant
            foreach_plist(constants, ScriptVariant, constIter)
            {
                if (c->constValue.vt == constIter.value()->vt &&
                    ScriptVariant_IsTrue(ScriptVariant_Eq(&c->constValue, constIter.value())))
                {
                    c->id = i;
                    break;
                }
                i++;
            }
            // constant doesn't exist yet; add it to the list
            if (c->id < 0)
            {
                assert(i == constants->size());
                constants->gotoLast();
                constants->insertAfter(&c->constValue);
                c->id = i;
            }
        }
    }
}

void doTest(char *scriptText, const char *filename)
{
    ExecBuilder execBuilder;
    pp_context ppContext;
    Parser parser;

    pp_context_init(&ppContext);
    parser.parseText(&ppContext, &execBuilder, scriptText, 1, filename);
    pp_context_destroy(&ppContext);

    foreach_list(execBuilder.ssaFunctions, SSABuilder, iter)
    {
        link(iter.value(), &execBuilder.ssaFunctions);
        compile(iter.value());
        linkConstants(iter.value(), &execBuilder.constants);
    }

    execBuilder.buildExecutable();
    execBuilder.printInstructions();
    
    if (execBuilder.interpreter->functions.findByName("main"))
    {
        ScriptVariant retval;
        printf("\n\nRunning function 'main'...\n");
        execBuilder.interpreter->runFunction(execBuilder.interpreter->functions.retrieve(), NULL, &retval);
        char buf[256];
        ScriptVariant_ToString(&retval, buf);
        printf("\nReturned value: %s\n", buf);
    }
}

bool testFile(const char *filename)
{
    char *scriptText;
    int fileSize;
    FILE *fp = fopen(filename, "rb");
    if(!fp) goto io_error;
    if (fseek(fp, 0, SEEK_END) < 0) goto io_error;
    fileSize = ftell(fp);
    scriptText = (char*)malloc(fileSize + 1);
    if (fseek(fp, 0, SEEK_SET) < 0) goto io_error;
    if (fread(scriptText, 1, fileSize, fp) != fileSize) goto io_error;
    scriptText[fileSize] = 0;
    
    doTest(scriptText, filename);
    
    free(scriptText);
    return true;

io_error:
    fprintf(stderr, "error reading file %s: %s\n", filename, strerror(errno));
    if (fp) fclose(fp);
    return false;
}

int main(int argc, char **argv)
{
    pp_context ppContext;
    Parser parser;
    List fakeIList;
    char *scriptText = (char*) // function calls and if/else
        "void func(int bar) {\n"
        "    int foo = bar++ * 3, baz = !(--bar);\n"
        "    foo + bar;\n" // dead code
        "    getSomeProperty(foo, bar, bar * baz, someFunction());\n"
        "\n"
        "    if (foo) { foo = foo + 4; }\n"
        "\n"
        "    if (bar) bar += 1;\n"
        "    else if (baz) bar += 2;\n"
        "    else bar += 3;\n"
        "\n"
        "    fakereturn(foo, bar);\n"
        "}\n";
    char *scriptText2 = (char *) // simple while loop
        "void func2(int bar) {\n"
        "    while(bar--) {\n"
        "        someCall();\n"
        "    }\n"
        "}\n";
    char *scriptText3 = (char *) // test while loops: break @ end
        "void whileBreakAtEnd() {\n"
        "    while(1) {\n"
        // "        if (--bar > 0) bar; //continue;\n"
        // "        else if (bar < 0) bar; //break;\n"
         // "        --bar;\n"
        "        someCall();\n"
        "        break;\n"
        "    }\n"
        "}\n";
    char *scriptText4 = (char *) // test while loops: break @ end
        "void whileContinue(int bar) {\n"
        "    while(bar) {\n"
        // "        if (--bar > 0) bar; //continue;\n"
        // "        else if (bar < 0) bar; //break;\n"
         // "        --bar;\n"
        "        if (--bar) continue;\n"
        "        someCall();\n"
        "    }\n"
        "}\n";
    // test TODO: do-while loops, return
    
    // testFile("test/2-simple-while-loop.c");
    // testFile("test/5-do-while.c");
    // testFile("test/6-for-loop.c");
    // testFile("test/7-empty-for.c");
    if (argc != 2)
    {
        fprintf(stderr, argc < 2 ? "no file specified\n" : "too many arguments\n");
        fprintf(stderr, "usage: %s script.c\n", argv[0]);
        return 1;
    }
    
    testFile(argv[1]);
    return 0;
}

