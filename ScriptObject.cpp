#include <stdio.h>
#include "globals.h"
#include "List.hpp"
#include "ScriptVariant.hpp"
#include "StrCache.hpp"
#include "ScriptObject.hpp"
#include "ObjectHeap.hpp"

// this has to be defined somewhere...since there's no ScriptContainer.cpp, let's put it here
ScriptContainer::~ScriptContainer()
{
}

ScriptObject::ScriptObject()
{
    persistent = false;
    currentlyPrinting = false;
}

// destructor that destroys all pointers in map
ScriptObject::~ScriptObject()
{
    if (persistent)
    {
        map.gotoFirst();
        while (map.size())
        {
            ScriptVariant_Unref(map.valuePtr());
            map.remove();
        }
    }
}

void ScriptObject::set(const char *key, const ScriptVariant *value)
{
    if (map.findByName(key))
    {
        if (persistent)
            ScriptVariant_Unref(map.valuePtr());
        map.update(*value);
    }
    else
    {
        map.gotoLast();
        map.insertAfter(*value, key);
    }
}

bool ScriptObject::get(ScriptVariant *dst, const ScriptVariant *key)
{
    if (key->vt == VT_STR)
    {
        if (map.findByName(StrCache_Get(key->strVal)))
        {
            *dst = map.retrieve();
            return true;
        }
        else
        {
            printf("error: object has no member named %s\n", StrCache_Get(key->strVal));
            return false;
        }
    }
    else
    {
        // TODO: include the invalid key in the error message
        printf("error: object key must be a string\n");
        return false;
    }
}

void ScriptObject::makePersistent()
{
    if (persistent) return;
    persistent = true; // set it up here to avoid infinite recursion in case of cycles
    foreach_list(map, ScriptVariant, iter)
    {
        ScriptVariant *var = iter.valuePtr();
        if (var->vt == VT_OBJECT || var->vt == VT_LIST)
        {
            //printf("make object %i persistent\n", var->objVal);
            var->objVal = ObjectHeap_Ref(var->objVal);
        }
        else if (var->vt == VT_STR)
        {
            //printf("make string %i persistent\n", var->strVal);
            var->strVal = StrCache_Ref(var->strVal);
        }
    }
}

void ScriptObject::print()
{
    char buf[256];
    bool first = true;

    if (currentlyPrinting)
    {
        printf("{(object cycle)}");
        return;
    }
    currentlyPrinting = true;

    printf("{");
    foreach_list(map, ScriptVariant, iter)
    {
        if (!first) printf(", ");
        first = false;
        printf("\"%s\": ", iter.name());
        ScriptVariant *var = iter.valuePtr();
        if (var->vt == VT_OBJECT) ObjectHeap_GetObject(var->objVal)->print();
        else if (var->vt == VT_LIST) ObjectHeap_GetList(var->objVal)->print();
        else
        {
            ScriptVariant_ToString(var, buf, sizeof(buf));
            printf("%s", buf);
        }
    }
    printf("}");
    currentlyPrinting = false;
}

int ScriptObject::toString(char *dst, int dstsize)
{
#define SNPRINTF(...) { int n = snprintf(dst, dstsize, __VA_ARGS__); dst += n; length += n; dstsize -= n; if (dstsize < 0) dstsize = 0; }
    char buf[256];
    bool first = true;
    int length = 0;

    if (currentlyPrinting)
    {
        SNPRINTF("{(object cycle)}");
        return length;
    }
    currentlyPrinting = true;

    SNPRINTF("{");
    foreach_list(map, ScriptVariant, iter)
    {
        if (!first) SNPRINTF(", ");
        first = false;
        SNPRINTF("\"%s\": ", iter.name());
        ScriptVariant *var = iter.valuePtr();
        ScriptVariant_ToString(var, buf, sizeof(buf));
        SNPRINTF("%s", buf);
    }
    SNPRINTF("}");
    currentlyPrinting = false;
    return length;
#undef SNPRINTF
}


