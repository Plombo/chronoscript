#include <stdio.h>
#include "List.h"
#include "ScriptVariant.h"
#include "StrCache.h"
#include "ScriptObject.h"

ScriptObject::ScriptObject()
{
    persistent = false;
    currentlyPrinting = false;
}

// destructor that destroys all pointers in map
ScriptObject::~ScriptObject()
{
    map.gotoFirst();
    while (map.size())
    {
        ScriptVariant_Unref(map.valuePtr());
        map.remove();
    }
}

// TODO make this take a const pointer to a ScriptVariant
void ScriptObject::set(const char *key, ScriptVariant value)
{
    if (persistent)
    {
        value = *ScriptVariant_Ref(&value);

        /* Write barrier: a black object cannot reference a white object, so
           turn the white object gray */
        if (value.vt == VT_OBJECT && // TODO: or VT_LIST, when lists are implemented
            gcColor == GC_COLOR_BLACK)
        {
            ScriptObject *valueObject = ObjectHeap_Get(value.objVal);
            if (valueObject->gcColor == GC_COLOR_WHITE)
            {
                // TODO add value.objVal to gray stack
            }
        }
    }

    if (map.findByName(key))
    {
        ScriptVariant_Unref(map.valuePtr());
        map.update(value);
    }
    else
    {
        map.gotoLast();
        map.insertAfter(value, key);
    }
}

ScriptVariant ScriptObject::get(const char *key)
{
    if (map.findByName(key))
    {
        return map.retrieve();
    }
    else
    {
        ScriptVariant retvar = {{.ptrVal = NULL}, VT_EMPTY};
        return retvar;
    }
}

void ScriptObject::makePersistent()
{
    if (persistent) return;
    persistent = true; // set it up here to avoid infinite recursion in case of cycles
    gcColor = GC_COLOR_WHITE;
    foreach_list(map, ScriptVariant, iter)
    {
        ScriptVariant *var = iter.valuePtr();
        if (var->vt == VT_OBJECT)
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
        if (var->vt == VT_OBJECT) ObjectHeap_Get(var->objVal)->print();
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


