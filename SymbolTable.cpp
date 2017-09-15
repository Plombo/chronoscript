/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2014 OpenBOR Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SymbolTable.h"


void Symbol_Init(Symbol *symbol, const char *theName, ScriptVariant *pvar)
{
    memset(symbol, 0, sizeof(Symbol));
    if(theName)
    {
        strcpy(symbol->name, theName);
    }
    else
    {
        symbol->name[0] = 0;
    }
    ScriptVariant_Init(&(symbol->var));
    if(pvar)
    {
        ScriptVariant_Copy(&(symbol->var), pvar);
    }
}

//------------------------------------------------------------------


SymbolTable::SymbolTable(const char *theName)
{
    nextSymbolCount = 0;
    if (theName)
    {
        strcpy(this->name, theName);
    }
    else
    {
        this->name[0] = 0;
    }
}


SymbolTable::~SymbolTable()
{
    foreach_list(symbolList, Symbol*, iter)
    {
        Symbol *psymbol = iter.value();
        if (psymbol)
        {
            ScriptVariant_Clear(&(psymbol->var));
            free(psymbol);
        }
    }
    symbolList.clear();
}


/******************************************************************************
*  FindSymbol -- Using the name of the symbol, this method searches the symbol
*  table.
*  Parameters: symbolName -- LPCOLESTR which contains the name of the desired
*                            symbol.
*              pp_theSymbol -- CSymbol** which receives the address of the
*                              CSymbol, or NULL if no symbol is found.
*  Returns: true if the symbol is found.
*           false otherwise.
******************************************************************************/
bool SymbolTable::findSymbol(const char *symbolName, Symbol **pp_theSymbol)
{
    if (symbolName && symbolList.findByName(symbolName))
    {
        *pp_theSymbol = symbolList.retrieve();
        return true;
    }
    else
    {
        return false;
    }
}

/******************************************************************************
*  AddSymbol -- This method adds a CSymbol* to the symbol table.
*  Parameters: p_theSymbol -- address of the CSymbol to be added to the symbol
*                             table.
*  Returns:
******************************************************************************/
void SymbolTable::addSymbol(Symbol *p_theSymbol)
{
    symbolList.insertAfter(p_theSymbol, p_theSymbol->name);
}


/******************************************************************************
*  CStackedSymbolTable -- This class handles the scoping issues related to
*  the symbol table, allowing correct resolution of variable references.
******************************************************************************/

/******************************************************************************
*  Initialize -- This method intializes the stacked symbol table with its name
*  and creates a new stack for the symbol tables.
******************************************************************************/
StackedSymbolTable::StackedSymbolTable()
{
    nextScopeNum = 0;
    pushScope();
}

StackedSymbolTable::~StackedSymbolTable()
{
    while (!symbolTableStack.isEmpty())
    {
        popScope();
    }
    nextScopeNum = 0;
}

/******************************************************************************
*  PushScope -- This method creates a new symbol table and pushes it onto the
*  symbol table stack.  The top symbol table on the stack represents the
*  innermost symbol scope.
******************************************************************************/
void StackedSymbolTable::pushScope()
{
    SymbolTable *newSymbolTable = NULL;
    char scopeName[32];

    //Name this scope with a unique (sequential) number
    snprintf(scopeName, sizeof(scopeName), "%i", nextScopeNum++);
    newSymbolTable = new SymbolTable(scopeName);

    //Add the new symboltable to the stack
    symbolTableStack.push(newSymbolTable);
}


/******************************************************************************
*  TopScope -- This method retrieves the topmost symbol table from the symbol
*  table stack, and passes it back to the caller without removing it.
******************************************************************************/
SymbolTable *StackedSymbolTable::topScope()
{
    return symbolTableStack.top();
}

/******************************************************************************
*  PopScope -- This method pops the topmost symbol table off the stack,
*  destroying it in the process.  Any symbols in this table have gone out of
*  scope and should not be found in a symbol search.
******************************************************************************/
void StackedSymbolTable::popScope()
{
    SymbolTable *pSymbolTable = symbolTableStack.top();
    symbolTableStack.pop();
    if (pSymbolTable)
    {
        delete pSymbolTable;
    }
}

/******************************************************************************
*  FindSymbol -- This method searches the symbol table for the symbol
*  we're looking for.
*  Parameters: symbolName -- const char *which contains the name of the desired
*                            symbol.
*              pp_theSymbol -- CSymbol** which receives the address of the
*                              CSymbol, or NULL if no symbol is found.
*  Returns: true if the symbol is found.
*           false otherwise.
******************************************************************************/
bool StackedSymbolTable::findSymbol(const char *symbolName, Symbol **pp_theSymbol, char *p_scopedName)
{
    SymbolTable *currentSymbolTable = NULL;
    bool found = false;
    foreach_list(symbolTableStack, SymbolTable*, iter)
    {
        currentSymbolTable = iter.value();
        found = currentSymbolTable->findSymbol(symbolName, pp_theSymbol);

        if (found)
        {
            sprintf(p_scopedName, "%s$%s", symbolName, currentSymbolTable->name);
            break;
        }
    }

    //Restore the stack so push and pop work correctly.
    symbolTableStack.gotoFirst();
    return found;
}

/******************************************************************************
*  AddSymbol -- This method adds a CSymbol* to the topmost symbol table on the
*  stack.
*  Parameters: p_theSymbol -- address of the CSymbol to be added to the symbol
*                             table.
*  Returns:
******************************************************************************/
void StackedSymbolTable::addSymbol(Symbol *p_theSymbol)
{
    symbolTableStack.top()->addSymbol(p_theSymbol);
}

