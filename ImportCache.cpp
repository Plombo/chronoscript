/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2016 OpenBOR Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "Interpreter.h"
#include "List.h"
// #include "packfile.h"
#include "ImportCache.h"

#include "ssa.h"
#include "Parser.h"
#include "liveness.h"
#include "regalloc.h"
#include "Builtins.h"
#include "ExecBuilder.h"
#include "pp_parser.h"

//#define IC_DEBUG 1

List<Interpreter*> compiledScripts; // names are lowercased, forward-slashed paths

/**
 * Reads a script file into an allocated buffer.  Be sure to call free() on the
 * returned buffer when you are done with it!
 *
 * Returns the buffer on success, NULL on failure.
 */
static char *readScript(const char *path)
{
#if 1 // stdio version
    char *scriptText = NULL;
    int fileSize;
    FILE *fp = fopen(path, "rb");
    if(!fp) goto error;
    if (fseek(fp, 0, SEEK_END) < 0) goto error;
    fileSize = ftell(fp);
    scriptText = (char*) malloc(fileSize + 1);
    if (fseek(fp, 0, SEEK_SET) < 0) goto error;
    if ((int)fread(scriptText, 1, fileSize, fp) != fileSize) goto error;
    scriptText[fileSize] = 0;
    fclose(fp);
    return scriptText;

error:
    // ideally, this error message would include the name of the file that tried to import this file
    printf("Script error: unable to open file '%s' for importing\n", path);
    if (scriptText)
    {
        free(scriptText);
    }
    if (fp)
    {
        fclose(fp);
    }
    return NULL;
#else // packfile I/O version
    int handle = openpackfile(path, packfile);
    int size;
    char *buffer = NULL;

    if(handle < 0)
    {
        goto error;
    }
    size = seekpackfile(handle, 0, SEEK_END);
    if(size < 0)
    {
        goto error;
    }
    buffer = malloc(size + 1);
    if(buffer == NULL)
    {
        goto error;
    }

    if(seekpackfile(handle, 0, SEEK_SET) < 0)
    {
        goto error;
    }
    if(readpackfile(handle, buffer, size) < 0)
    {
        goto error;
    }
    closepackfile(handle);
    buffer[size] = '\0';
    return buffer;

error:
    // ideally, this error message would include the name of the file that tried to import this file
    printf("Script error: unable to open file '%s' for importing\n", path);
    if(buffer)
    {
        free(buffer);
    }
    if(handle >= 0)
    {
        closepackfile(handle);
    }
    return NULL;
#endif
}

bool compile(SSABuilder *func)
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
#if DEBUG_RA
    printf("\n%i temporaries\n\n", func->temporaries.size());
#endif

    computeLiveSets(func);
#if DEBUG_RA
    printf("\nLive sets:\n");
    foreach_list(func->basicBlockList, BasicBlock*, iter)
    {
        BasicBlock *block = iter.value();
        printf("BB %i: in: ", block->id);
        block->liveIn.print();
        printf(", out:");
        block->liveOut.print();
        printf("\n");
    }
#endif
    
    LivenessAnalyzer livenessAnalyzer(func);
    livenessAnalyzer.computeLiveIntervals();
    livenessAnalyzer.coalesce();
    livenessAnalyzer.buildInterferenceGraph();
    
    RegAlloc registerAllocator(livenessAnalyzer.values, livenessAnalyzer.uniqueNodes);
    registerAllocator.run();
#if DEBUG_RA
    for (int i = 0; i < livenessAnalyzer.uniqueNodes; i++)
    {
        printf("node %i -> $%i\n", livenessAnalyzer.values[i]->id, livenessAnalyzer.values[i]->color);
    }
#endif
    
    // assign registers
    foreach_list(func->temporaries, Temporary*, iter)
    {
        Temporary *value = iter.value();
        value->reg = livenessAnalyzer.nodeForTemp[value->id]->root()->color;
    }

#if DEBUG_RA
    // print instruction list again
    printf("\n");
    func->printInstructionList();
#endif

    // give instructions their final indices
    int nextIndex = 0;
    foreach_list(func->instructionList, Instruction*, iter)
    {
        Instruction *inst = iter.value();
        if (inst->op == OP_BB_START)
            inst->block->startIndex = nextIndex;
        if (inst->isTrivial())
            iter.remove();
        else
            inst->seqIndex = nextIndex++;
    }

    // print the final instruction list
    printf("\nFinal instruction list:\n");
    foreach_list(func->instructionList, Instruction*, iter)
    {
        iter.value()->print();
    }
    printf("\n");

    // check for uses of undefined values
    foreach_list(func->instructionList, Instruction*, iter)
    {
        foreach_list(iter.value()->operands, RValue*, rvalue_iter)
        {
            if (rvalue_iter.value()->isUndefined())
            {
                printf("Error: possible use of undefined value\n");
                return false;
            }
        }
    }

    return true;
}

bool link(SSABuilder *func, List<ExecFunction*> *localFunctions, List<Interpreter*> *imports)
{
    foreach_list(func->instructionList, Instruction*, iter)
    {
        Instruction *inst = iter.value();
        if (inst->op != OP_CALL) continue;
        FunctionCall *call = inst->asFunctionCall();
        if (localFunctions->findByName(call->functionName))
        {
            call->functionRef = localFunctions->retrieve();
            printf("linked call to %s\n", call->functionName);
        }
        else if ((call->functionRef = ImportList_GetFunctionPointer(imports, call->functionName)))
            printf("linked call to %s (imported)\n", call->functionName);
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
            {
                printf("Error: couldn't link %s\n", call->functionName);
                return false;
            }
        }
    }
    return true;
}

void linkConstants(SSABuilder *func, List<ScriptVariant*> *constants)
{
    foreach_list(func->instructionList, Instruction*, instIter)
    {
        Instruction *inst = instIter.value();
        foreach_list(inst->operands, RValue*, srcIter)
        {
            if (!srcIter.value()->isConstant()) continue;
            Constant *c = srcIter.value()->asConstant();
            int i = 0;
            // try to use an existing constant
            foreach_plist(constants, ScriptVariant*, constIter)
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
                if (c->constValue.vt == VT_STR)
                    c->constValue.strVal = StrCache_Ref(c->constValue.strVal);
            }
        }
    }
    // get the string cache to free unused string constants
    foreach_list(func->constantList, Constant*, constIter)
    {
        ScriptVariant *var = &constIter.value()->constValue;
        if (var->vt == VT_STR)
            StrCache_Unref(var->strVal);
    }
}

/**
 * Loads and compiles a script file.
 */
Interpreter *compileFile(const char *filename)
{
    char *scriptText = readScript(filename);
    if (!scriptText) return NULL;

    ExecBuilder execBuilder;
    pp_context ppContext;
    List<Interpreter*> imports;
    int numImports;

    Parser parser(&ppContext, &execBuilder, scriptText, 1, filename);
    parser.parseText();
    if (parser.errorFound()) goto error;

    execBuilder.allocateExecFunctions();
    compiledScripts.gotoLast();
    compiledScripts.insertAfter(execBuilder.interpreter, filename);

    // get imports
    numImports = ppContext.imports.size();
    ppContext.imports.gotoFirst();
    for (int i = 0; i < numImports; i++)
    {
        Interpreter *importedScript = ImportCache_ImportFile(ppContext.imports.getName());
        if (importedScript == NULL)
        {
            goto error;
        }
        printf("imported script %s => %p\n", ppContext.imports.getName(), importedScript);
        imports.insertAfter(importedScript);
        ppContext.imports.gotoNext();
    }
    ppContext.clear();

    foreach_list(execBuilder.ssaFunctions, SSABuilder*, iter)
    {
        SSABuilder *func = iter.value();
        if (!link(func, &execBuilder.interpreter->functions, &imports)) goto error;
        if (!compile(func)) goto error;
        linkConstants(func, &execBuilder.constants);
    }

    execBuilder.buildExecutable();
    ralloc_free(parser.memCtx);
    free(scriptText);
    execBuilder.printInstructions();
    return execBuilder.interpreter;

error:
    printf("Failed to compile script '%s'.\n", filename);
    delete execBuilder.interpreter;
    if (compiledScripts.findByName(filename))
    {
        compiledScripts.remove();
    }
    ralloc_free(parser.memCtx);
    free(scriptText);
    return NULL;
}

/**
 * From a list of interpreters, finds and returns a pointer to the function with
 * the specified name in the list. Script files at the *end* of the import list
 * have priority, so if the author imports two script files with the same
 * function name, the function used will be from the file imported last.
 */
ExecFunction *ImportList_GetFunctionPointer(List<Interpreter*> *list, const char *name)
{
    list->gotoLast();
    for (int i = list->size(); i > 0; i--)
    {
        Interpreter *node = list->retrieve();
        ExecFunction *func = node->getFunctionNamed(name);
        if(func != NULL)
        {
#ifdef IC_DEBUG
            fprintf(stderr, "importing '%s' from '%s'\n", name, list->getName());
#endif
            return func;
        }
        list->gotoPrevious();
    }
    return NULL;
}

/**
 * Initializes the import cache.
 */
void ImportCache_Init()
{
    ImportCache_Clear();
}

/**
 * Returns a pointer to the interpreter for the specified script.
 *
 * If the file has already been imported, this function returns the pointer to
 * the existing node.  If it hasn't, it imports it and then returns the pointer.
 *
 * Returns NULL if the file hasn't already been imported and an error occurs
 * when compiling it.
 */
Interpreter *ImportCache_ImportFile(const char *path)
{
    int i;
    char path2[256];

    // first convert the path to standard form, lowercase with forward slashes
    assert(strlen(path) <= 255);
    for(i = strlen(path); i >= 0; i--)
    {
        if(path[i] == '\\')
        {
            path2[i] = '/';
        }
        else if(path[i] >= 'A' && path[i] <= 'Z')
        {
            path2[i] = path[i] + ('a' - 'A');
        }
        else
        {
            path2[i] = path[i];
        }
    }
#ifdef IC_DEBUG
    fprintf(stderr, "ImportCache_ImportFile: '%s' -> '%s'\n", path, path2);
#endif

    // find and return interpreter if this file has already been imported
    if (compiledScripts.findByName(path2))
        return compiledScripts.retrieve();
    // otherwise, compile the script and return the newly created interpreter
    else
        return compileFile(path2);
}

/**
 * Frees all of the imported scripts. Called when shutting down the script
 * engine.
 */
void ImportCache_Clear()
{
    foreach_list(compiledScripts, Interpreter*, iter)
    {
        delete iter.value();
    }
    compiledScripts.clear();
}


