#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include "globals.h"
#include "Builtins.hpp"
#include "ScriptVariant.hpp"
#include "List.hpp"
#include "ObjectHeap.hpp"

static bool builtinsInited = false;
static List<unsigned int> builtinIndices;
static List<unsigned int> methodIndices;
static ScriptVariant globalsObject = {{.ptrVal = NULL}, .vt = VT_EMPTY};

extern int script_arg_count;
extern char **script_args;

// mark the globals object as referenced for GC purposes
void pushGlobalVariantsToGC()
{
    // this check is necessary because the globals object might not be initialized yet
    if (globalsObject.vt == VT_OBJECT)
    {
        GarbageCollector_PushGray(globalsObject.objVal);
    }
}

static CCResult log_builtin_common(int numParams, ScriptVariant *params, ScriptVariant *retval, bool newline)
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
    return CC_OK;
}

// log([a, [b, [...]]])
// writes each of its parameters to the log file, separated by a space and followed by a newline
CCResult builtin_log(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    return log_builtin_common(numParams, params, retval, true);
}

// log_write([a, [b, [...]]])
// writes each of its parameters to the log file, separated by a space, not followed by a newline
CCResult builtin_log_write(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    return log_builtin_common(numParams, params, retval, false);
}

// get_args()
// returns the command line arguments of "runscript" as a list
CCResult builtin_get_args(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 0)
    {
        printf("Error: get_args() takes no parameters\n");
        return CC_FAIL;
    }

    int list = ObjectHeap_CreateNewList(script_arg_count);
    for (int i = 0; i < script_arg_count; i++)
    {
        int len = strlen(script_args[i]);
        ScriptVariant stringVar;
        stringVar.strVal = StrCache_Pop(len);
        stringVar.vt = VT_STR;
        snprintf(StrCache_Get(stringVar.strVal), len + 1, "%s", script_args[i]);
        StrCache_SetHash(stringVar.strVal);
        ObjectHeap_SetListMember(list, i, &stringVar);
    }

    retval->objVal = list;
    retval->vt = VT_LIST;

    return CC_OK;
}

// list_append(list, value)
// adds the given value to the end of the list
CCResult builtin_list_append(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 2)
    {
        printf("Error: list_append(list, value) requires exactly 2 parameters\n");
        return CC_FAIL;
    }
    else if (params[0].vt != VT_LIST)
    {
        printf("Error: list_append: first parameter must be a list\n");
        return CC_FAIL;
    }

    if (!ObjectHeap_InsertInList(params[0].objVal, ObjectHeap_GetList(params[0].objVal)->size(), &params[1]))
    {
        printf("Error: list_append failed\n");
        return CC_FAIL;
    }

    retval->vt = VT_EMPTY;
    return CC_OK;
}

// list_insert(list, position, value)
// inserts the given value in the list at the given position
CCResult builtin_list_insert(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 3)
    {
        printf("Error: list_insert(list, position, value) requires exactly 3 parameters\n");
        return CC_FAIL;
    }
    else if (params[0].vt != VT_LIST)
    {
        printf("Error: list_insert: first parameter must be a list\n");
        return CC_FAIL;
    }
    else if (params[1].vt != VT_INTEGER)
    {
        printf("Error: list_insert: second parameter must be an integer\n");
        return CC_FAIL;
    }
    else if (params[1].lVal < 0)
    {
        printf("Error: list_insert: position cannot be negative\n");
        return CC_FAIL;
    }

    if (!ObjectHeap_InsertInList(params[0].objVal, params[1].lVal, &params[2]))
    {
        printf("Error: list_insert failed\n");
        return CC_FAIL;
    }

    retval->vt = VT_EMPTY;
    return CC_OK;
}

// list_length(list)
// returns the length of the list as an integer
CCResult builtin_list_length(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 1)
    {
        printf("Error: list_length(list) requires exactly 1 parameter\n");
        return CC_FAIL;
    }
    else if (params[0].vt != VT_LIST)
    {
        printf("Error: list_length: parameter must be a list\n");
        return CC_FAIL;
    }

    retval->lVal = ObjectHeap_GetList(params[0].objVal)->size();
    retval->vt = VT_INTEGER;

    return CC_OK;
}

// list_remove(list, position)
// removes the element at the given position from the list
CCResult builtin_list_remove(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 2)
    {
        printf("Error: list_remove(list, position) requires exactly 2 parameters\n");
        return CC_FAIL;
    }
    else if (params[0].vt != VT_LIST)
    {
        printf("Error: list_remove: first parameter must be a list\n");
        return CC_FAIL;
    }
    else if (params[1].vt != VT_INTEGER)
    {
        printf("Error: list_remove: second parameter must be an integer\n");
        return CC_FAIL;
    }
    else if (params[1].lVal < 0)
    {
        printf("Error: list_remove: position cannot be negative\n");
        return CC_FAIL;
    }

    if (!ObjectHeap_GetList(params[0].objVal)->remove(params[1].lVal))
    {
        printf("Error: list_remove failed\n");
        return CC_FAIL;
    }

    retval->vt = VT_EMPTY;
    return CC_OK;
}

// globals()
// returns object holding global variants
CCResult builtin_globals(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 0)
    {
        printf("Error: globals() takes no parameters\n");
        return CC_FAIL;
    }

    if (globalsObject.vt == VT_EMPTY)
    {
        // globals object doesn't exist yet; create it
        globalsObject.objVal = ObjectHeap_CreateNewObject(8);
        globalsObject.vt = VT_OBJECT;
        ScriptVariant_Ref(&globalsObject);
    }

    *retval = globalsObject;
    return CC_OK;
}

// string_char_at(string, index)
// returns the character (as an integer)
CCResult builtin_string_char_at(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 2)
    {
        printf("Error: string_char_at(string, index) requires exactly 2 parameters\n");
        return CC_FAIL;
    }
    else if (params[0].vt != VT_STR)
    {
        printf("Error: string_char_at(string, index): first parameter must be a string\n");
        return CC_FAIL;
    }
    else if (params[1].vt != VT_INTEGER)
    {
        printf("Error: string_char_at(string, index): second parameter must be an integer\n");
        return CC_FAIL;
    }

    const char *string = StrCache_Get(params[0].strVal);
    int index = params[1].lVal;

    if (index < 0)
    {
        printf("Error: string_char_at: index (%i) is negative\n", index);
        return CC_FAIL;
    }
    else if (index >= StrCache_Len(params[0].strVal))
    {
        printf("Error: string_char_at: index (%i) >= string length (%i)\n", index, StrCache_Len(params[0].strVal));
        return CC_FAIL;
    }

    retval->vt = VT_INTEGER;
    retval->lVal = string[index];

    return CC_OK;
}

// string_length(string)
// returns the length of the string
CCResult builtin_string_length(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 1)
    {
        printf("Error: string_length(string) requires exactly 1 parameter\n");
        return CC_FAIL;
    }
    else if (params[0].vt != VT_STR)
    {
        printf("Error: string_length(string): parameter must be a string\n");
        return CC_FAIL;
    }

    retval->vt = VT_INTEGER;
    retval->lVal = StrCache_Len(params[0].strVal);

    return CC_OK;
}

// to_decimal(value)
// converts the value to a double
//      for integers: convert to double
//      for decimals: no-op
//      for strings: parse double
CCResult builtin_to_decimal(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 1)
    {
        printf("Error: to_decimal(value) requires exactly 1 parameter\n");
        return CC_FAIL;
    }

    if (params[0].vt == VT_INTEGER)
    {
        retval->dblVal = (double) params[0].lVal;
        retval->vt = VT_DECIMAL;
        return CC_OK;
    }
    else if (params[0].vt == VT_DECIMAL)
    {
        *retval = params[0];
        return CC_OK;
    }
    else if (params[0].vt == VT_STR)
    {
        // Parse double-precision floating point value from string.
        const char *str = StrCache_Get(params[0].strVal);
        char *endptr;
        errno = 0;
        double val = strtod(str, &endptr);
        if (errno == ERANGE)
        {
            printf("Error: '%s' is too large or small to fit in a decimal (double-precision float)\n", str);
            return CC_FAIL;
        }
        else if (endptr == str || *endptr != '\0')
        {
            printf("Error: '%s' is not a number\n", str);
            return CC_FAIL;
        }
        else if (errno != 0)
        {
            printf("Error: when converting '%s' to decimal: %s\n", str, strerror(errno));
            return CC_FAIL;
        }

        retval->dblVal = val;
        retval->vt = VT_DECIMAL;
        return CC_OK;
    }

    printf("Error: to_decimal(value) only accepts an integer, decimal, or string as its parameter\n");
    return CC_FAIL;
}

// to_integer(value)
// converts the value to an integer
//      for integers: no-op
//      for decimals: truncate
//      for strings: parse integer
CCResult builtin_to_integer(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 1)
    {
        printf("Error: to_integer(value) requires exactly 1 parameter\n");
        return CC_FAIL;
    }

    switch (params[0].vt)
    {
        case VT_INTEGER:
        {
            *retval = params[0];
            return CC_OK;
        }
        case VT_DECIMAL:
        {
            // truncate floating-point value, as long as it's in the range of a signed 32-bit integer
            double dbl = params[0].dblVal;
            if (isnan(dbl) || isinf(dbl))
            {
                printf("Error: cannot convert NaN or infinity to an integer\n");
                return CC_FAIL;
            }
            else if (dbl < INT32_MIN || dbl > INT32_MAX)
            {
                printf("Error: %f is too large or small to fit in a 32-bit integer\n", dbl);
                return CC_FAIL;
            }

            retval->lVal = (int32_t) dbl;
            retval->vt = VT_INTEGER;
            return CC_OK;
        }
        case VT_STR:
        {
            // Parse 32-bit int from string. Accept values that would fit in a 32-bit signed or unsigned int, but
            // not anything that would overflow 32 bits.
            const char *str = StrCache_Get(params[0].strVal);
            char *endptr;
            errno = 0;
            int64_t int64val = strtoll(str, &endptr, 10);
            if (errno == ERANGE || int64val < INT32_MIN || int64val > UINT32_MAX)
            {
                printf("Error: '%s' is too large or small to fit in a 32-bit integer\n", str);
                return CC_FAIL;
            }
            else if (endptr == str || *endptr != '\0')
            {
                printf("Error: '%s' is not an integer\n", str);
                return CC_FAIL;
            }
            else if (errno != 0 && int64val == 0)
            {
                printf("Error: when converting '%s' to integer: %s\n", str, strerror(errno));
                return CC_FAIL;
            }

            retval->lVal = (int32_t) int64val;
            retval->vt = VT_INTEGER;
            return CC_OK;
        }
        default:
        {
            printf("Error: to_integer(value) only accepts an integer, decimal, or string as its parameter\n");
            return CC_FAIL;
        }
    }
}

// char_from_integer(ascii)
// returns a 1-character string with the character described by the ASCII value
CCResult builtin_char_from_integer(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 1)
    {
        printf("Error: char_from_integer(ascii) requires exactly 1 parameter\n");
        return CC_FAIL;
    }
    else if (params[0].vt != VT_INTEGER)
    {
        printf("Error: char_from_integer(ascii): parameter must be an integer\n");
        return CC_FAIL;
    }

    int stringIndex = StrCache_Pop(1);
    char *string = StrCache_Get(stringIndex);
    string[0] = params[0].lVal;
    string[1] = 0;
    StrCache_SetHash(stringIndex);

    retval->strVal = stringIndex;
    retval->vt = VT_STR;

    return CC_OK;
}

// file_read(path)
// reads in an entire file and returns it as a string
CCResult builtin_file_read(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 1)
    {
        printf("Error: file_read(path) requires exactly 1 parameter\n");
        return CC_FAIL;
    }
    else if (params[0].vt != VT_STR)
    {
        printf("Error: file_read(path): parameter must be a string\n");
        return CC_FAIL;
    }

    const char *path = StrCache_Get(params[0].strVal);
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
    {
        printf("Error: failed to open file '%s'\n", path);
        return CC_FAIL;
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
        return CC_FAIL;
    }

    StrCache_SetHash(stringIndex);
    retval->strVal = stringIndex;
    retval->vt = VT_STR;

    return CC_OK;
}


// string methods
CCResult method_char_at(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    return builtin_string_char_at(numParams, params, retval);
}

// list methods
CCResult method_append(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    return builtin_list_append(numParams, params, retval);
}

CCResult method_insert(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    return builtin_list_insert(numParams, params, retval);
}

CCResult method_remove(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    return builtin_list_remove(numParams, params, retval);
}

// object methods
CCResult method_has_key(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (params[0].vt != VT_OBJECT)
    {
        printf("error: only objects have the has_key() method\n");
        return CC_FAIL;
    }
    else if (numParams != 2)
    {
        // the object the method is called on is included in numParams, so use numParams-1 in the error message
        printf("error: object.has_key(key) takes 1 argument, got %d instead\n", numParams - 1);
        return CC_FAIL;
    }
    // no need to check that params[1] is a string; the function will just return false in that case

    ScriptObject *obj = ObjectHeap_GetObject(params[0].objVal);
    retval->lVal = (int32_t) obj->hasKey(&params[1]);
    retval->vt = VT_INTEGER;
    return CC_OK;
}

CCResult method_keys(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (params[0].vt != VT_OBJECT)
    {
        printf("error: only objects have the keys() method\n");
        return CC_FAIL;
    }
    else if (numParams != 1)
    {
        // the object the method is called on is included in numParams, so use numParams-1 in the error message
        printf("error: object.has_key(key) takes no arguments, got %d instead\n", numParams - 1);
        return CC_FAIL;
    }

    ScriptObject *obj = ObjectHeap_GetObject(params[0].objVal);
    retval->objVal = obj->createKeysList();
    retval->vt = VT_LIST;
    return CC_OK;
}


struct Builtin {
    BuiltinScriptFunction function;
    const char *name;
};

#define DEF_BUILTIN(name) { builtin_##name, #name }
// define each builtin IN ALPHABETICAL ORDER or binary search won't work
static Builtin builtinsArray[] = {
    DEF_BUILTIN(char_from_integer),
    DEF_BUILTIN(file_read),
    DEF_BUILTIN(get_args),
    DEF_BUILTIN(globals),
    DEF_BUILTIN(list_append),
    DEF_BUILTIN(list_insert),
    DEF_BUILTIN(list_length),
    DEF_BUILTIN(list_remove),
    DEF_BUILTIN(log),
    DEF_BUILTIN(log_write),
    DEF_BUILTIN(string_char_at),
    DEF_BUILTIN(string_length),
    DEF_BUILTIN(to_decimal),
    DEF_BUILTIN(to_integer),
};
#undef DEF_BUILTIN

#define DEF_METHOD(name) { method_##name, #name }
static Builtin methodsArray[] = {
    DEF_METHOD(append),
    DEF_METHOD(char_at),
    DEF_METHOD(has_key),
    DEF_METHOD(insert),
    DEF_METHOD(keys),
    DEF_METHOD(remove),
};
#undef DEF_METHOD


// initialize builtin lists for public use
static void initBuiltins()
{
    if (builtinsInited) return;
    const unsigned int numBuiltins = sizeof(builtinsArray) / sizeof(Builtin);
    for (unsigned int i = 0; i < numBuiltins; i++)
    {
        builtinIndices.insertAfter(i, builtinsArray[i].name);
    }

    const unsigned int numMethods = sizeof(methodsArray) / sizeof(Builtin);
    for (unsigned int i = 0; i < numMethods; i++)
    {
        methodIndices.insertAfter(i, methodsArray[i].name);
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

// returns index of method with the given name, or -1 if it doesn't exist
int getMethodIndex(const char *methodName)
{
    if (!builtinsInited) initBuiltins();
    if (methodIndices.findByName(methodName))
    {
        return methodIndices.retrieve();
    }
    else return -1;
}

// returns the method with the given index
BuiltinScriptFunction getMethodByIndex(int index)
{
    return methodsArray[index].function;
}

const char *getMethodName(int index)
{
    return methodsArray[index].name;
}

