#ifndef BUILTINS_H
#define BUILTINS_H

#include "depends.h"
#include "ScriptVariant.h"

typedef HRESULT (*BuiltinScriptFunction)(int numParams, ScriptVariant *params, ScriptVariant *retval);

// returns index of builtin with the given name, or -1 if it doesn't exist
int getBuiltinIndex(const char *functionName);

// returns the function with the given index
BuiltinScriptFunction getBuiltinByIndex(int index);

// returns the name of the function with the given index
const char *getBuiltinName(int index);

#endif
