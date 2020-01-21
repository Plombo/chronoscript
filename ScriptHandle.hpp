#ifndef SCRIPT_HANDLE_HPP
#define SCRIPT_HANDLE_HPP

// Enumeration of every internal engine type accessible from script.
enum ScriptHandleType
{
    SH_TYPE_INVALID = 0,
    SH_TYPE_ANIMATION = 1,
    SH_TYPE_MODEL = 2,
    SH_TYPE_ENTITY = 3,
};

// Every type in the engine must inherit from this class in order to be accessible as a script handle.
class ScriptHandle
{
public:
    ScriptHandleType scriptHandleType;

    inline ScriptHandle(ScriptHandleType type) : scriptHandleType(type) {}
};

// Below are dummy "internal engine" classes since ChronoScript is currently standalone and not part of a game engine.

#include <stdio.h> // for snprintf
class Model : public ScriptHandle
{
public:
    char name[64];

    inline Model(const char *theName) : ScriptHandle(SH_TYPE_MODEL)
    {
        snprintf(name, sizeof(name), "%s", theName);
    }
};

struct fvec3 { float x, y, z; };
class Entity : public ScriptHandle
{
public:
    fvec3 position;
    fvec3 velocity;
    const Model *model;

    inline Entity(const Model *theModel, float xPos, float yPos, float zPos) : ScriptHandle(SH_TYPE_ENTITY)
    {
        model = theModel;
        position.x = xPos;
        position.y = yPos;
        position.z = zPos;
        velocity.x = velocity.y = velocity.z = 0.0f;
    }
};

#endif

