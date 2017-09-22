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

void Symbol_Init(Symbol *symbol, const char *theName, ScriptVariant *pvar);

// symbol table for a single scope
class SymbolTable
{
public:
    List<Symbol*> symbolList;
    int  nextSymbolCount;
    char name[MAX_STR_LEN + 1];

    SymbolTable(const char *theName);
    ~SymbolTable();
    bool findSymbol(const char *symbolName, Symbol **pp_theSymbol);
    void addSymbol(Symbol *p_theSymbol);
};

// stack of symbol tables for all scopes
class StackedSymbolTable
{
public:
    Stack<SymbolTable*> symbolTableStack;
    int nextScopeNum;

    StackedSymbolTable();
    ~StackedSymbolTable();
    void pushScope();
    SymbolTable *topScope();
    void popScope();
    bool findSymbol(const char *symbolName, Symbol **pp_theSymbol);
    void addSymbol(Symbol *p_theSymbol);
};

#endif
