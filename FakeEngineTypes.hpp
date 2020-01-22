// These are "internal engine" classes since ChronoScript is currently standalone and not part of a game engine.
#ifndef FAKE_ENGINE_TYPES_HPP
#define FAKE_ENGINE_TYPES_HPP

#include <stdio.h> // for snprintf
#include "ScriptHandle.hpp"
#include "ScriptVariant.hpp"

class Model : public ScriptHandle
{
public:
    char name[64];

    inline Model(const char *theName) : ScriptHandle(SH_TYPE_MODEL)
    {
        snprintf(name, sizeof(name), "%s", theName);
    }

    CCResult getScriptProperty(int propertyName, ScriptVariant *result) override;
    CCResult setScriptProperty(int propertyName, const ScriptVariant *value) override;
};

struct fvec3 { float x, y, z; };
class Entity : public ScriptHandle
{
public:
    fvec3 position;
    fvec3 velocity;
    Model *model;

    inline Entity(Model *theModel, float xPos, float yPos, float zPos) : ScriptHandle(SH_TYPE_ENTITY)
    {
        model = theModel;
        position.x = xPos;
        position.y = yPos;
        position.z = zPos;
        velocity.x = velocity.y = velocity.z = 0.0f;
    }

    CCResult getScriptProperty(int propertyName, ScriptVariant *result) override;
    CCResult setScriptProperty(int propertyName, const ScriptVariant *value) override;
};

#endif

