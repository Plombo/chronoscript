#include "Builtins.h"
#include "List.h"

static bool builtinsInited = false;
static CList<void> builtinIndices;

// log([a, [b, [...]]])
// writes each of its parameters to the log file, separated by a space
HRESULT builtin_log(int numParams, ScriptVariant *params, ScriptVariant *retval)
{
    char buf[MAX_STR_VAR_LEN + 1];
    for (int i = 0; i < numParams; i++)
    {
        if (i > 0) printf(" ");
        ScriptVariant_ToString(&params[i], buf);
        printf("%s", buf);
    }
    printf("\n");

    // no return value
    retval->vt = VT_EMPTY;
    retval->ptrVal = NULL;
    return S_OK;
}

#define DEF_BUILTIN(name) { builtin_##name, #name }
struct Builtin {
    BuiltinScriptFunction function;
    const char *name;
};
// define each builtin IN ALPHABETICAL ORDER or binary search won't work
static Builtin builtinsArray[] = {
    DEF_BUILTIN(log),
};
#undef DEF_BUILTIN

// initialize builtin lists for public use
static void initBuiltins()
{
    if (builtinsInited) return;
    int numBuiltins = sizeof(builtinsArray) / sizeof(Builtin);

    for (size_t i = 0; i < numBuiltins; i++)
    {
        builtinIndices.insertAfter((void*)i, builtinsArray[i].name);
    }

    builtinsInited = true;
}

// returns index of builtin with the given name, or -1 if it doesn't exist
int getBuiltinIndex(const char *functionName)
{
    if (!builtinsInited) initBuiltins();
    if (builtinIndices.findByName(functionName))
        return (int)(size_t)builtinIndices.retrieve();
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

