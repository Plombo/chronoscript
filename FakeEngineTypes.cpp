#include <string.h>
#include "globals.h"
#include "ScriptHandle.hpp"
#include "ScriptVariant.hpp"
#include "FakeEngineTypes.hpp"
#include "stringhash.h"

// A more optimal implementation would compare the name's hash in the string cache against precomputed values.
CCResult Model::getScriptProperty(int propertyNameIndex, ScriptVariant *result)
{
    const char *propertyName = StrCache_Get(propertyNameIndex);
    
    if (strcmp(propertyName, "name") == 0)
    {
        size_t length = strlen(this->name);
        int newStrIndex = StrCache_Pop(length);
        char *newString = StrCache_Get(newStrIndex);
        memcpy(newString, this->name, length);
        newString[length] = '\0';
        StrCache_SetHash(newStrIndex);
        result->strVal = newStrIndex;
        result->vt = VT_STR;
        return CC_OK;
    }
    else
    {
        printf("error: models don't have a property named '%s'\n", StrCache_Get(propertyNameIndex));
        result->ptrVal = NULL;
        result->vt = VT_EMPTY;
        return CC_FAIL;
    }
}

CCResult Model::setScriptProperty(int propertyNameIndex, const ScriptVariant *value)
{
    printf("error: models are read-only; their properties can't be changed\n");
    return CC_FAIL;
}

CCResult Entity::getScriptProperty(int propertyNameIndex, ScriptVariant *result)
{
    const char *propertyName = StrCache_Get(propertyNameIndex);

    if (strcmp(propertyName, "model") == 0)
    {
        result->ptrVal = this->model;
        result->vt = VT_PTR;
        return CC_OK;
    }
    else if (strcmp(propertyName, "pos_x") == 0)
    {
        result->dblVal = this->position.x;
        result->vt = VT_DECIMAL;
        return CC_OK;
    }
    else if (strcmp(propertyName, "pos_y") == 0)
    {
        result->dblVal = this->position.y;
        result->vt = VT_DECIMAL;
        return CC_OK;
    }
    else if (strcmp(propertyName, "pos_z") == 0)
    {
        result->dblVal = this->position.z;
        result->vt = VT_DECIMAL;
        return CC_OK;
    }
    else if (strcmp(propertyName, "vx") == 0)
    {
        result->dblVal = this->velocity.x;
        result->vt = VT_DECIMAL;
        return CC_OK;
    }
    else if (strcmp(propertyName, "vy") == 0)
    {
        result->dblVal = this->velocity.y;
        result->vt = VT_DECIMAL;
        return CC_OK;
    }
    else if (strcmp(propertyName, "vz") == 0)
    {
        result->dblVal = this->velocity.z;
        result->vt = VT_DECIMAL;
        return CC_OK;
    }
    else
    {
        printf("error: entities don't have a property named '%s'\n", StrCache_Get(propertyNameIndex));
        result->ptrVal = NULL;
        result->vt = VT_EMPTY;
        return CC_FAIL;
    }
}

CCResult Entity::setScriptProperty(int propertyNameIndex, const ScriptVariant *value)
{
    const char *propertyName = StrCache_Get(propertyNameIndex);
    double theValue;

    if (strcmp(propertyName, "vx") == 0)
    {
        if (ScriptVariant_DecimalValue(value, &theValue) == CC_FAIL)
        {
            printf("error: the value of the '%s' property must be a number\n", propertyName);
            return CC_FAIL;
        }
        this->velocity.x = theValue;
        return CC_OK;
    }
    else if (strcmp(propertyName, "vy") == 0)
    {
        if (ScriptVariant_DecimalValue(value, &theValue) == CC_FAIL)
        {
            printf("error: the value of the '%s' property must be a number\n", propertyName);
            return CC_FAIL;
        }
        this->velocity.y = theValue;
        return CC_OK;
    }
    else if (strcmp(propertyName, "vz") == 0)
    {
        if (ScriptVariant_DecimalValue(value, &theValue) == CC_FAIL)
        {
            printf("error: the value of the '%s' property must be a number\n", propertyName);
            return CC_FAIL;
        }
        this->velocity.z = theValue;
        return CC_OK;
    }

    printf("error: entities don't have a property named '%s', or it's read-only\n", propertyName);
    return CC_FAIL;
}

