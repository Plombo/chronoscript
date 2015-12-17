/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2014 OpenBOR Team
 */

#ifndef SYMBOLTABLE_H
#define SYMBOLTABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ScriptVariant.h"
#include "List.h"
#include "Stack.h"

typedef struct Symbol
{
    char  name[MAX_STR_LEN + 1];
    ScriptVariant var;
} Symbol;

// symbol table for a single scope
typedef struct SymbolTable
{
    List SymbolList;
    int  nextSymbolCount;
    char name[MAX_STR_LEN + 1];
} SymbolTable;

void Symbol_Init(Symbol *symbol, const char *theName, ScriptVariant *pvar);
void SymbolTable_Init(SymbolTable *stable, const char *theName);
void SymbolTable_Clear(SymbolTable *stable);
BOOL SymbolTable_FindSymbol(SymbolTable *stable, const char *symbolName, Symbol **pp_theSymbol);
void SymbolTable_AddSymbol(SymbolTable *stable, Symbol *p_theSymbol);

// stack of symbol tables for all scopes
typedef struct StackedSymbolTable
{
    Stack SymbolTableStack;
    int nextScopeNum;
} StackedSymbolTable;

void StackedSymbolTable_Init(StackedSymbolTable *sstable);
void StackedSymbolTable_Clear(StackedSymbolTable *sstable);
void StackedSymbolTable_PushScope(StackedSymbolTable *sstable);
SymbolTable *StackedSymbolTable_TopScope(StackedSymbolTable *sstable);
void StackedSymbolTable_PopScope(StackedSymbolTable *sstable);
BOOL StackedSymbolTable_FindSymbol(StackedSymbolTable *sstable, const char *symbolName,
                                   Symbol **pp_theSymbol, char *p_scopedName);
void StackedSymbolTable_AddSymbol(StackedSymbolTable *sstable, Symbol *p_theSymbol);

#ifdef __cplusplus
}; // extern "C"
#endif

#endif
