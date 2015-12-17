/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2014 OpenBOR Team
 */

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


void SymbolTable_Init(SymbolTable *stable, const char *theName)
{
    List_Init(&(stable->SymbolList));
    stable->nextSymbolCount = 0;
    if(theName)
    {
        strcpy(stable->name, theName);
    }
    else
    {
        stable->name[0] = 0;
    }
}


void SymbolTable_Clear(SymbolTable *stable)
{
    Symbol *psymbol = NULL;
    int i, size;
    FOREACH( stable->SymbolList,
             psymbol = (Symbol *)List_Retrieve(&(stable->SymbolList));
             if(psymbol)
{
    ScriptVariant_Clear(&(psymbol->var));
        free(psymbol);
    }
           );
    List_Clear(&(stable->SymbolList));
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
BOOL SymbolTable_FindSymbol(SymbolTable *stable, const char *symbolName, Symbol **pp_theSymbol )
{
    if (symbolName && List_FindByName(&(stable->SymbolList), (char *) symbolName ))
    {
        *pp_theSymbol = (Symbol *)List_Retrieve(&(stable->SymbolList));
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/******************************************************************************
*  AddSymbol -- This method adds a CSymbol* to the symbol table.
*  Parameters: p_theSymbol -- address of the CSymbol to be added to the symbol
*                             table.
*  Returns:
******************************************************************************/
void SymbolTable_AddSymbol(SymbolTable *stable, Symbol *p_theSymbol )
{
    List_InsertAfter( &(stable->SymbolList), (void *)p_theSymbol, p_theSymbol->name);
}


/******************************************************************************
*  CStackedSymbolTable -- This class handles the scoping issues related to
*  the symbol table, allowing correct resolution of variable references.
******************************************************************************/

/******************************************************************************
*  Initialize -- This method intializes the stacked symbol table with its name
*  and creates a new stack for the symbol tables.
******************************************************************************/
void StackedSymbolTable_Init(StackedSymbolTable *sstable)
{
    List_Init(&(sstable->SymbolTableStack));
    sstable->nextScopeNum = 0;
    StackedSymbolTable_PushScope(sstable);
}

void StackedSymbolTable_Clear(StackedSymbolTable *sstable)
{
    while(!Stack_IsEmpty(&(sstable->SymbolTableStack)))
    {
        StackedSymbolTable_PopScope(sstable);
    }
    List_Clear(&(sstable->SymbolTableStack));
    sstable->nextScopeNum = 0;
}

/******************************************************************************
*  PushScope -- This method creates a new symbol table and pushes it onto the
*  symbol table stack.  The top symbol table on the stack represents the
*  innermost symbol scope.
******************************************************************************/
void StackedSymbolTable_PushScope(StackedSymbolTable *sstable)
{
    SymbolTable *newSymbolTable = NULL;
    char scopeName[32];

    newSymbolTable = (SymbolTable *)malloc(sizeof(SymbolTable));
    //Name this scope with a unique (sequential) number
    snprintf(scopeName, sizeof(scopeName), "%i", sstable->nextScopeNum++);
    SymbolTable_Init(newSymbolTable, scopeName);

    //Add the new symboltable to the stack
    Stack_Push(&(sstable->SymbolTableStack), (void *)newSymbolTable);
}


/******************************************************************************
*  TopScope -- This method retrieves the topmost symbol table from the symbol
*  table stack, and passes it back to the caller without removing it.
******************************************************************************/
SymbolTable *StackedSymbolTable_TopScope(StackedSymbolTable *sstable)
{
    return (SymbolTable *)Stack_Top(&(sstable->SymbolTableStack));
}

/******************************************************************************
*  PopScope -- This method pops the topmost symbol table off the stack,
*  destroying it in the process.  Any symbols in this table have gone out of
*  scope and should not be found in a symbol search.
******************************************************************************/
void StackedSymbolTable_PopScope(StackedSymbolTable *sstable)
{
    SymbolTable *pSymbolTable = NULL;
    pSymbolTable = (SymbolTable *)Stack_Top(&(sstable->SymbolTableStack));
    Stack_Pop(&(sstable->SymbolTableStack));
    if(pSymbolTable)
    {
        SymbolTable_Clear(pSymbolTable);
        free((void *)pSymbolTable);
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
BOOL StackedSymbolTable_FindSymbol(StackedSymbolTable *sstable, const char *symbolName,
                                   Symbol **pp_theSymbol, char *p_scopedName)
{
    SymbolTable *currentSymbolTable = NULL;
    BOOL bFound = FALSE;
    int i, size;
    FOREACH( sstable->SymbolTableStack,
             currentSymbolTable = (SymbolTable *)List_Retrieve(&(sstable->SymbolTableStack));
             bFound = SymbolTable_FindSymbol(currentSymbolTable, symbolName, pp_theSymbol );

             if(bFound)
             {
                 sprintf(p_scopedName, "%s$%s", symbolName, currentSymbolTable->name);
                 break;
             }
           );

    //Restore the stack so push and pop work correctly.
    List_Reset(&(sstable->SymbolTableStack));
    return bFound;
}

/******************************************************************************
*  AddSymbol -- This method adds a CSymbol* to the topmost symbol table on the
*  stack.
*  Parameters: p_theSymbol -- address of the CSymbol to be added to the symbol
*                             table.
*  Returns:
******************************************************************************/
void StackedSymbolTable_AddSymbol(StackedSymbolTable *sstable, Symbol *p_theSymbol )
{
    SymbolTable *topSymbolTable = NULL;
    topSymbolTable = (SymbolTable *)Stack_Top(&(sstable->SymbolTableStack));
    SymbolTable_AddSymbol(topSymbolTable, p_theSymbol );
}
