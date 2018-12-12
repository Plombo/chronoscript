#include <stdio.h>
#include <stdlib.h>
#include "Builtins.hpp"
#include "List.hpp"
#include "ObjectHeap.hpp"

static bool builtinsInited = false;
static List<unsigned int> builtinIndices;
static List<ScriptVariant> globalVariants;

// mark script objects in the global variant list as referenced
void pushGlobalVariantsToGC()
{
    foreach_list(globalVariants, ScriptVariant, iter)
    {
        ScriptVariant *var = iter.valuePtr();
        if (var->vt == VT_OBJECT && var->objVal >= 0)
        {
            GarbageCollector_PushGray(var->objVal);
        }
    }
}

// log([a, [b, [...]]])
// writes each of its parameters to the log file, separated by a space
HRESULT builtin_log(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    char buf[MAX_STR_VAR_LEN + 1];
    for (int i = 0; i < numParams; i++)
    {
        if (i > 0) printf(" ");
        ScriptVariant_ToString(&params[i], buf, sizeof(buf));
        printf("%s", buf);
    }
    printf("\n");

    // no return value
    retval->vt = VT_EMPTY;
    retval->ptrVal = NULL;
    return S_OK;
}

// list_append(list, value)
// adds the given value to the end of the list
HRESULT builtin_list_append(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 2)
    {
        printf("Error: list_append(list, value) requires exactly 2 parameters\n");
        return E_FAIL;
    }
    else if (params[0].vt != VT_LIST)
    {
        printf("Error: list_append: first parameter must be a list\n");
        return E_FAIL;
    }

    if (!ObjectHeap_InsertInList(params[0].objVal, ObjectHeap_GetList(params[0].objVal)->size(), &params[1]))
    {
        printf("Error: list_append failed\n");
        return E_FAIL;
    }

    return S_OK;
}

// list_insert(list, position, value)
// inserts the given value in the list at the given position
HRESULT builtin_list_insert(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 3)
    {
        printf("Error: list_insert(list, position, value) requires exactly 3 parameters\n");
        return E_FAIL;
    }
    else if (params[0].vt != VT_LIST)
    {
        printf("Error: list_insert: first parameter must be a list\n");
        return E_FAIL;
    }
    else if (params[1].vt != VT_INTEGER)
    {
        printf("Error: list_insert: second parameter must be an integer\n");
        return E_FAIL;
    }
    else if (params[1].lVal < 0)
    {
        printf("Error: list_insert: position cannot be negative\n");
    }

    if (!ObjectHeap_InsertInList(params[0].objVal, params[1].lVal, &params[2]))
    {
        printf("Error: list_insert failed\n");
        return E_FAIL;
    }

    return S_OK;
}

// get_global_variant(name)
// returns global variant with the given name, or fails if no such variant exists
HRESULT builtin_get_global_variant(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 1)
    {
        printf("Error: get_global_variant(name) requires exactly 1 parameter\n");
        return E_FAIL;
    }
    else if (params[0].vt != VT_STR)
    {
        printf("Error: get_global_variant: name parameter must be a string\n");
        return E_FAIL;
    }

    const char *name = StrCache_Get(params[0].strVal);
    if (globalVariants.findByName(name))
    {
        *retval = globalVariants.retrieve();
        return S_OK;
    }
    else
    {
        printf("Error: get_global_variant: no global variant named '%s'\n", name);
        return E_FAIL;
    }
}

// set_global_variant(name, value)
// sets global variant with the given name and value
HRESULT builtin_set_global_variant(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 2)
    {
        printf("Error: set_global_variant(name, value) requires exactly 2 parameters\n");
        return E_FAIL;
    }
    else if (params[0].vt != VT_STR)
    {
        printf("Error: set_global_variant: name parameter must be a string\n");
        return E_FAIL;
    }

    /* Ref the new value to make it persistent -- must be done before unreffing
       the old value, because the old and new values could be the same
       string/list/object! */
    ScriptVariant *reffedValue = ScriptVariant_Ref(&params[1]);
    const char *name = StrCache_Get(params[0].strVal);
    if (globalVariants.findByName(name))
    {
        ScriptVariant oldValue = globalVariants.retrieve();
        ScriptVariant_Unref(&oldValue);
        globalVariants.update(*reffedValue);
    }
    else
    {
        globalVariants.gotoLast(); // not strictly necessary, but nice
        globalVariants.insertAfter(*reffedValue, name);
    }

    return S_OK;
}

#define DEF_BUILTIN(name) { builtin_##name, #name }
struct Builtin {
    BuiltinScriptFunction function;
    const char *name;
};
// define each builtin IN ALPHABETICAL ORDER or binary search won't work
static Builtin builtinsArray[] = {
    DEF_BUILTIN(get_global_variant),
    DEF_BUILTIN(list_append),
    DEF_BUILTIN(list_insert),
    DEF_BUILTIN(log),
    DEF_BUILTIN(set_global_variant),
};
#undef DEF_BUILTIN

// initialize builtin lists for public use
static void initBuiltins()
{
    if (builtinsInited) return;
    size_t numBuiltins = sizeof(builtinsArray) / sizeof(Builtin);

    for (unsigned int i = 0; i < numBuiltins; i++)
    {
        builtinIndices.insertAfter(i, builtinsArray[i].name);
    }

    builtinsInited = true;
}

// returns index of builtin with the given name, or -1 if it doesn't exist
int getBuiltinIndex(const char *functionName)
{
    if (!builtinsInited) initBuiltins();
    if (builtinIndices.findByName(functionName))
        return builtinIndices.retrieve();
    else return -1;
}

// returns the function with the given index
BuiltinScriptFunction getBuiltinByIndex(int index)
{
    return builtinsArray[index].function;
}

const char *getBuiltinName(int index)
{
    return builtinsArray[index].name;
}

