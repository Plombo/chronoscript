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
#include "FakeEngineTypes.hpp"

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

// this function is used by SSABuilder for constant folding, as well as in the actual cc_constant() implementation
CCResult scriptConstantValue(const ScriptVariant *nameVar, ScriptVariant *result)
{
    if (nameVar->vt != VT_STR)
    {
        return CC_FAIL;
    }

    const char *constName = StrCache_Get(nameVar->strVal);

    #define ICMPCONST(x) \
        else if(strcmp(#x, constName)==0) {\
	        result->lVal = x;\
	        result->vt = VT_INTEGER;\
	        return CC_OK;\
        }

    // some sample constants pulled from openbor.h
    #define COMPATIBLEVERSION	0x00033748
    #define MAX_ENTS     150
    #define	MAX_SPECIALS 8

    if (0) {}
    ICMPCONST(COMPATIBLEVERSION)
    ICMPCONST(MAX_ENTS)
    ICMPCONST(MAX_SPECIALS)

    // if we reach here, there was no match for the constant name
    return CC_FAIL;
}

// cc_constant(constant_name)
// returns value of the engine constant with the given name
CCResult builtin_cc_constant(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 1)
    {
        printf("Error: cc_constant(constant_name) requires exactly 1 parameter\n");
        return CC_FAIL;
    }
    else if (params[0].vt != VT_STR)
    {
        printf("Error: cc_constant(): first parameter must be a string\n");
        return CC_FAIL;
    }

    if (scriptConstantValue(&params[0], retval) == CC_FAIL)
    {
        printf("Error: cc_constant(): no constant named '%s'\n", StrCache_Get(params[0].strVal));
    }
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

// to_string(value)
// converts the value to a string
CCResult builtin_to_string(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 1)
    {
        printf("Error: to_string(value) requires exactly 1 parameter\n");
        return CC_FAIL;
    }
    else if (params[0].vt == VT_STR)
    {
        *retval = params[0];
        return CC_OK;
    }

    int length = ScriptVariant_ToString(&params[0], NULL, 0);
    int strCacheIndex = StrCache_Pop(length);
    ScriptVariant_ToString(&params[0], StrCache_Get(strCacheIndex), length + 1);
    StrCache_SetHash(strCacheIndex);
    retval->strVal = strCacheIndex;
    retval->vt = VT_STR;
    return CC_OK;
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

// create_model(name)
// creates a model with the given name
CCResult builtin_create_model(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 1)
    {
        printf("Error: create_model(name) requires exactly 1 parameter\n");
        return CC_FAIL;
    }
    else if (params[0].vt != VT_STR)
    {
        printf("Error: create_model(name): parameter must be a string\n");
        return CC_FAIL;
    }

    const char *name = StrCache_Get(params[0].strVal);
    Model *model = new Model(name);
    retval->ptrVal = model;
    retval->vt = VT_PTR;
    return CC_OK;
}

// create_entity(model, x, y, z)
// creates a new entity
CCResult builtin_create_entity(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 4)
    {
        printf("Error: create_entity(model, xpos, ypos, zpos) requires exactly 4 parameters\n");
        return CC_FAIL;
    }
    else if (params[0].vt != VT_PTR || params[0].ptrVal->scriptHandleType != SH_TYPE_MODEL)
    {
        printf("Error: create_entity(): first parameter must be a model\n");
        return CC_FAIL;
    }

    double x, y, z;
    if (ScriptVariant_DecimalValue(&params[1], &x) == CC_FAIL ||
        ScriptVariant_DecimalValue(&params[2], &y) == CC_FAIL ||
        ScriptVariant_DecimalValue(&params[1], &z) == CC_FAIL)
    {
        printf("Error: create_entity(): parameters 2-4 must be numbers\n");
        return CC_FAIL;
    }

    Model *model = static_cast<Model*>(params[0].ptrVal);
    Entity *entity = new Entity(model, x, y, z);
    retval->ptrVal = entity;
    retval->vt = VT_PTR;
    return CC_OK;
}


// string methods
CCResult method_char_at(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    return builtin_string_char_at(numParams, params, retval);
}

// string.substring(start[, end])
// returns the substring of characters from start (inclusive) to end (exclusive)
CCResult method_substring(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    // this is a method, so we're guaranteed at least 1 param, so accessing params[0] here is safe
    if (params[0].vt != VT_STR)
    {
        printf("Error: only strings have the substring() method\n");
        return CC_FAIL;
    }
    else if (numParams != 2 && numParams != 3)
    {
        printf("Error: the substring(start[, end]) method requires 1 or 2 parameters, not %i\n", numParams);
        return CC_FAIL;
    }
    else if (params[1].vt != VT_INTEGER || (numParams > 2 && params[2].vt != VT_INTEGER))
    {
        printf("Error: the parameter(s) of the substring() method must be integers\n");
        return CC_FAIL;
    }

    int sourceLength = StrCache_Len(params[0].strVal);
    const char *sourceString = StrCache_Get(params[0].strVal);
    int start = params[1].lVal;
    int end = (numParams > 2) ? params[2].lVal : sourceLength;
    int length = end - start;

    if (start < 0 || start >= sourceLength)
    {
        printf("Error: start position %i is not a valid index in the string '%s' with length %i\n",
            start, sourceString, sourceLength);
        return CC_FAIL;
    }
    else if (end < start)
    {
        printf("Error: end (%i) is before start (%i)\n", end, start);
        return CC_FAIL;
    }
    else if (end > sourceLength)
    {
        printf("Error: end (%i) is beyond the end of the string '%s' with length %i\n",
            end, sourceString, sourceLength);
        return CC_FAIL;
    }

    int newStrIndex = StrCache_Pop(length);
    char *newString = StrCache_Get(newStrIndex);
    memcpy(newString, sourceString + start, length);
    newString[length] = '\0';
    StrCache_SetHash(newStrIndex);

    retval->strVal = newStrIndex;
    retval->vt = VT_STR;
    return CC_OK;
}

// string.length() / list.length()
CCResult method_length(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 1)
    {
        printf("Error: the length() method takes no parameters\n");
        return CC_FAIL;
    }

    if (params[0].vt == VT_STR)
    {
        retval->lVal = StrCache_Len(params[0].strVal);
        retval->vt = VT_INTEGER;
        return CC_OK;
    }
    else if (params[0].vt == VT_LIST)
    {
        retval->lVal = ObjectHeap_GetList(params[0].objVal)->size();
        retval->vt = VT_INTEGER;
        return CC_OK;
    }
    else
    {
        printf("Error: invalid type for length() method\n");
        return CC_FAIL;
    }
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

// entity.move(x, y, z)
// "moves" the fake entity type by the given delta
CCResult method_move(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    if (numParams != 4)
    {
        printf("Error: entity.move(dx, dy, dz) requires exactly 3 parameters\n");
        return CC_FAIL;
    }
    else if (params[0].vt != VT_PTR || params[0].ptrVal->scriptHandleType != SH_TYPE_ENTITY)
    {
        printf("Error: move() method called on something that isn't an entity\n");
        return CC_FAIL;
    }

    double dx, dy, dz;
    if (ScriptVariant_DecimalValue(&params[1], &dx) == CC_FAIL ||
        ScriptVariant_DecimalValue(&params[2], &dy) == CC_FAIL ||
        ScriptVariant_DecimalValue(&params[1], &dz) == CC_FAIL)
    {
        printf("Error: move(): parameters 1-3 must be numbers\n");
        return CC_FAIL;
    }

    Entity *entity = static_cast<Entity*>(params[0].ptrVal);
    entity->position.x += dx;
    entity->position.y += dy;
    entity->position.z += dz;

    retval->ptrVal = NULL;
    retval->vt = VT_EMPTY;
    return CC_OK;
}


struct Builtin {
    BuiltinScriptFunction function;
    const char *name;
};

#define DEF_BUILTIN(name) { builtin_##name, #name }
// define each builtin IN ALPHABETICAL ORDER or binary search won't work
static Builtin builtinsArray[] = {
    DEF_BUILTIN(cc_constant),
    DEF_BUILTIN(char_from_integer),
    DEF_BUILTIN(create_entity),
    DEF_BUILTIN(create_model),
    DEF_BUILTIN(file_read),
    DEF_BUILTIN(get_args),
    DEF_BUILTIN(globals),
    DEF_BUILTIN(list_append),
    DEF_BUILTIN(list_insert),
    DEF_BUILTIN(list_remove),
    DEF_BUILTIN(log),
    DEF_BUILTIN(log_write),
    DEF_BUILTIN(string_char_at),
    DEF_BUILTIN(to_decimal),
    DEF_BUILTIN(to_integer),
    DEF_BUILTIN(to_string),
};
#undef DEF_BUILTIN

#define DEF_METHOD(name) { method_##name, #name }
static Builtin methodsArray[] = {
    DEF_METHOD(append),
    DEF_METHOD(char_at),
    DEF_METHOD(has_key),
    DEF_METHOD(insert),
    DEF_METHOD(keys),
    DEF_METHOD(length),
    DEF_METHOD(move),
    DEF_METHOD(remove),
    DEF_METHOD(substring),
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

