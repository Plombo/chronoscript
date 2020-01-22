#ifndef SCRIPT_HANDLE_HPP
#define SCRIPT_HANDLE_HPP

#include "depends.h"

// Enumeration of every internal engine type accessible from script.
enum ScriptHandleType
{
    SH_TYPE_INVALID = 0,
    SH_TYPE_ANIMATION = 1,
    SH_TYPE_MODEL = 2,
    SH_TYPE_ENTITY = 3,
};

struct ScriptVariant; // defined in ScriptVariant.hpp, which can't be included here since it includes this header

// Every type in the engine must inherit from this class in order to be accessible as a script handle.
class ScriptHandle
{
public:
    ScriptHandleType scriptHandleType;

    inline ScriptHandle(ScriptHandleType type) : scriptHandleType(type) {}

    // propertyName is the index of the property name in the string cache
    virtual CCResult getScriptProperty(int propertyName, ScriptVariant *result) = 0;
    virtual CCResult setScriptProperty(int propertyName, const ScriptVariant *value) = 0;
};

#endif

