/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2014 OpenBOR Team
 */

#ifndef SYMBOLTABLE_H
#define SYMBOLTABLE_H

#include "ScriptVariant.h"
#include "List.h"
#include "Stack.h"

struct Symbol
{
    char  name[MAX_STR_LEN + 1];
    ScriptVariant var;
};

// symbol table for a single scope
class SymbolTable
{
public:
    CList<Symbol> symbolList;
    int  nextSymbolCount;
    char name[MAX_STR_LEN + 1];

    SymbolTable(const char *theName);
    ~SymbolTable();
    bool findSymbol(const char *symbolName, Symbol **pp_theSymbol);
    void addSymbol(Symbol *p_theSymbol);
};

void Symbol_Init(Symbol *symbol, const char *theName, ScriptVariant *pvar);
void SymbolTable_Init(SymbolTable *stable, const char *theName);
void SymbolTable_Clear(SymbolTable *stable);
BOOL SymbolTable_FindSymbol(SymbolTable *stable, const char *symbolName, Symbol **pp_theSymbol);
void SymbolTable_AddSymbol(SymbolTable *stable, Symbol *p_theSymbol);

// stack of symbol tables for all scopes
class StackedSymbolTable
{
public:
    CStack<SymbolTable> symbolTableStack;
    int nextScopeNum;

    StackedSymbolTable();
    ~StackedSymbolTable();
    void pushScope();
    SymbolTable *topScope();
    void popScope();
    bool findSymbol(const char *symbolName, Symbol **pp_theSymbol, char *p_scopedName);
    void addSymbol(Symbol *p_theSymbol);
};

void StackedSymbolTable_Init(StackedSymbolTable *sstable);
void StackedSymbolTable_Clear(StackedSymbolTable *sstable);
void StackedSymbolTable_PushScope(StackedSymbolTable *sstable);
SymbolTable *StackedSymbolTable_TopScope(StackedSymbolTable *sstable);
void StackedSymbolTable_PopScope(StackedSymbolTable *sstable);
BOOL StackedSymbolTable_FindSymbol(StackedSymbolTable *sstable, const char *symbolName,
                                   Symbol **pp_theSymbol, char *p_scopedName);
void StackedSymbolTable_AddSymbol(StackedSymbolTable *sstable, Symbol *p_theSymbol);

#endif
