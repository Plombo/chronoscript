#include <stdio.h>
#include <stdlib.h>
#include "Builtins.hpp"
#include "List.hpp"
#include "ObjectHeap.hpp"

static bool builtinsInited = false;
static List<unsigned int> builtinIndices;
static List<ScriptVariant> globalVariants;
extern size_t script_arg_count;
extern char **script_args;

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

static HRESULT log_builtin_common(int numParams, ScriptVariant *params, ScriptVariant *retval, bool newline)
{
    char buf[MAX_STR_VAR_LEN + 1];
    for (int i = 0; i < numParams; i++)
    {
        if (i > 0) printf(" ");
        ScriptVariant_ToString(&params[i], buf, sizeof(buf));
        printf("%s", buf);
    }

    if (newline)
    {
        printf("\n");
    }
    fflush(stdout);

    // no return value
    retval->vt = VT_EMPTY;
    retval->ptrVal = NULL;
    return S_OK;
}

// log([a, [b, [...]]])
// writes each of its parameters to the log file, separated by a space and followed by a newline
HRESULT builtin_log(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    return log_builtin_common(numParams, params, retval, true);
}

// log_write([a, [b, [...]]])
// writes each of its parameters to the log file, separated by a space, not followed by a newline
HRESULT builtin_log_write(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    return log_builtin_common(numParams, params, retval, false);
}

// get_args()
// returns the command line arguments of "runscript" as a list
HRESULT builtin_get_args(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 0)
    {
        printf("Error: get_args() takes no parameters\n");
        return E_FAIL;
    }

    int list = ObjectHeap_CreateNewList(script_arg_count);
    for (size_t i = 0; i < script_arg_count; i++)
    {
        int len = strlen(script_args[i]);
        ScriptVariant stringVar;
        stringVar.strVal = StrCache_Pop(len);
        stringVar.vt = VT_STR;
        snprintf(StrCache_Get(stringVar.strVal), len + 1, "%s", script_args[i]);
        ObjectHeap_SetListMember(list, i, &stringVar);
    }

    retval->objVal = list;
    retval->vt = VT_LIST;

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

// list_length(list)
// returns the length of the list as an integer
HRESULT builtin_list_length(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 1)
    {
        printf("Error: list_length(list) requires exactly 1 parameter\n");
        return E_FAIL;
    }
    else if (params[0].vt != VT_LIST)
    {
        printf("Error: list_length: parameter must be a list\n");
        return E_FAIL;
    }

    retval->lVal = ObjectHeap_GetList(params[0].objVal)->size();
    retval->vt = VT_INTEGER;

    return S_OK;
}

// list_remove(list, position)
// removes the element at the given position from the list
HRESULT builtin_list_remove(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 2)
    {
        printf("Error: list_remove(list, position) requires exactly 2 parameters\n");
        return E_FAIL;
    }
    else if (params[0].vt != VT_LIST)
    {
        printf("Error: list_remove: first parameter must be a list\n");
        return E_FAIL;
    }
    else if (params[1].vt != VT_INTEGER)
    {
        printf("Error: list_remove: second parameter must be an integer\n");
        return E_FAIL;
    }
    else if (params[1].lVal < 0)
    {
        printf("Error: list_remove: position cannot be negative\n");
    }

    if (!ObjectHeap_GetList(params[0].objVal)->remove(params[1].lVal))
    {
        printf("Error: list_remove failed\n");
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

// string_char_at(string, index)
// returns the character (as an integer)
HRESULT builtin_string_char_at(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 2)
    {
        printf("Error: string_char_at(string, index) requires exactly 2 parameters\n");
        return E_FAIL;
    }
    else if (params[0].vt != VT_STR)
    {
        printf("Error: string_char_at(string, index): first parameter must be a string\n");
        return E_FAIL;
    }
    else if (params[1].vt != VT_INTEGER)
    {
        printf("Error: string_char_at(string, index): second parameter must be an integer\n");
        return E_FAIL;
    }

    const char *string = StrCache_Get(params[0].strVal);
    int index = params[1].lVal;

    if (index < 0)
    {
        printf("Error: string_char_at: index (%i) is negative\n", index);
        return E_FAIL;
    }
    else if (index >= StrCache_Len(params[0].strVal))
    {
        printf("Error: string_char_at: index (%i) >= string length (%i)\n", index, StrCache_Len(params[0].strVal));
        return E_FAIL;
    }

    retval->vt = VT_INTEGER;
    retval->lVal = string[index];

    return S_OK;
}

// string_length(string)
// returns the length of the string
HRESULT builtin_string_length(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 1)
    {
        printf("Error: string_length(string) requires exactly 1 parameter\n");
        return E_FAIL;
    }
    else if (params[0].vt != VT_STR)
    {
        printf("Error: string_length(string): parameter must be a string\n");
        return E_FAIL;
    }

    retval->vt = VT_INTEGER;
    retval->lVal = StrCache_Len(params[0].strVal);

    return S_OK;
}

// char_from_integer(ascii)
// returns a 1-character string with the character described by the ASCII value
HRESULT builtin_char_from_integer(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 1)
    {
        printf("Error: char_from_integer(ascii) requires exactly 1 parameter\n");
        return E_FAIL;
    }
    else if (params[0].vt != VT_INTEGER)
    {
        printf("Error: char_from_integer(ascii): parameter must be an integer\n");
        return E_FAIL;
    }

    int stringIndex = StrCache_Pop(1);
    char *string = StrCache_Get(stringIndex);
    string[0] = params[0].lVal;
    string[1] = 0;

    retval->strVal = stringIndex;
    retval->vt = VT_STR;

    return S_OK;
}

// file_read(path)
// reads in an entire file and returns it as a string
HRESULT builtin_file_read(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 1)
    {
        printf("Error: file_read(path) requires exactly 1 parameter\n");
        return E_FAIL;
    }
    else if (params[0].vt != VT_STR)
    {
        printf("Error: file_read(path): parameter must be a string\n");
        return E_FAIL;
    }

    const char *path = StrCache_Get(params[0].strVal);
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
    {
        printf("Error: failed to open file '%s'\n", path);
        return E_FAIL;
    }

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    int stringIndex = StrCache_Pop(size);
    char *string = StrCache_Get(stringIndex);
    while (size > 0 && !feof(fp) && !ferror(fp))
    {
        size_t bytes_read = fread(string, 1, size, fp);
        size -= bytes_read;
        string += bytes_read;
    }

    if (ferror(fp))
    {
        printf("Error: failed to read from file '%s'\n", path);
        fclose(fp);
        return E_FAIL;
    }

    retval->strVal = stringIndex;
    retval->vt = VT_STR;

    return S_OK;
}

#define DEF_BUILTIN(name) { builtin_##name, #name }
struct Builtin {
    BuiltinScriptFunction function;
    const char *name;
};
// define each builtin IN ALPHABETICAL ORDER or binary search won't work
static Builtin builtinsArray[] = {
    DEF_BUILTIN(char_from_integer),
    DEF_BUILTIN(file_read),
    DEF_BUILTIN(get_args),
    DEF_BUILTIN(get_global_variant),
    DEF_BUILTIN(list_append),
    DEF_BUILTIN(list_insert),
    DEF_BUILTIN(list_length),
    DEF_BUILTIN(list_remove),
    DEF_BUILTIN(log),
    DEF_BUILTIN(log_write),
    DEF_BUILTIN(set_global_variant),
    DEF_BUILTIN(string_char_at),
    DEF_BUILTIN(string_length),
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

