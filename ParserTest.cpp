#include <stdio.h>
#include "pp_parser.h"
#include "Parser.h"
#include "List.h"
#include "regalloc.h"
#include "liveness.h"

void doTest(char *scriptText, const char *filename)
{
    pp_context ppContext;
    Parser parser;
    List fakeIList;

    pp_context_init(&ppContext);
    List_Init(&fakeIList);
    parser.parseText(&ppContext, &fakeIList, scriptText, 1, filename);
    pp_context_destroy(&ppContext);

    printf("Instructions before processing:\n");
    parser.bld->printInstructionList();
    while(parser.bld->removeDeadCode());
    parser.bld->prepareForRegAlloc();
    printf("\nInstructions after processing:\n");
    parser.bld->printInstructionList();
    printf("\n%i temporaries\n\n", parser.bld->temporaries.size());
    
    computeLiveSets(parser.bld);
    printf("\nLive sets:\n");
    foreach_list(parser.bld->basicBlockList, BasicBlock, iter)
    {
        BasicBlock *block = iter.value();
        printf("BB %i: in: ", block->id);
        block->liveIn.print();
        printf(", out:", block->id);
        block->liveOut.print();
        printf("\n");
    }
    
    LivenessAnalyzer livenessAnalyzer(parser.bld);
    livenessAnalyzer.computeLiveIntervals();
    livenessAnalyzer.coalesce();
    livenessAnalyzer.buildInterferenceGraph();
    
    printf("\nOrdering: ");
    InterferenceNode **ordering = MCS(livenessAnalyzer.values, livenessAnalyzer.uniqueNodes);
    for (int i = 0; ordering[i] != NULL; i++)
    {
        printf("%i ", ordering[i]->id);
    }
    printf("\n\n");
    greedyColoring(ordering, livenessAnalyzer.uniqueNodes);
    for (int i = 0; i < livenessAnalyzer.uniqueNodes; i++)
    {
        printf("node %i -> $%i\n", livenessAnalyzer.values[i]->id, livenessAnalyzer.values[i]->color);
    }
    
    // assign registers
    foreach_list(parser.bld->temporaries, Temporary, iter)
    {
        Temporary *value = iter.value();
        value->reg = livenessAnalyzer.nodeForTemp[value->id]->color;
    }

    // print instruction list again
    parser.bld->printInstructionList();
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

