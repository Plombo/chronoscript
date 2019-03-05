#ifndef BUILTINS_HPP
#define BUILTINS_HPP

#include "depends.h"
#include "ScriptVariant.hpp"

typedef CCResult (*BuiltinScriptFunction)(int numParams, ScriptVariant *params, ScriptVariant *retval);

// returns index of builtin with the given name, or -1 if it doesn't exist
int getBuiltinIndex(const char *functionName);

// returns the function with the given index
BuiltinScriptFunction getBuiltinByIndex(int index);

// returns the name of the function with the given index
const char *getBuiltinName(int index);

// mark script objects in the global variant list as referenced
void pushGlobalVariantsToGC();

#endif

