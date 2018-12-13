#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ScriptList.hpp"
#include "ObjectHeap.hpp"

ScriptList::~ScriptList()
{
    if (persistent)
    {
        size_t size = storage.size(), i;
        for (i = 0; i < size; i++)
        {
            ScriptVariant_Unref(storage.getPtr(i));
        }
    }
}

void ScriptList::makePersistent()
{
    if (persistent) return;
    size_t size = storage.size(), i;
    persistent = true; // set it up here to avoid infinite recursion in case of cycles
    for (i = 0; i < size; i++)
    {
        ScriptVariant *var = storage.getPtr(i);
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

void ScriptList::print()
{
    char buf[256];
    bool first = true;

    if (currentlyPrinting)
    {
        printf("[(list cycle)]");
        return;
    }
    currentlyPrinting = true;

    printf("[");
    for (size_t i = 0; i < storage.size(); i++)
    {
        if (!first) printf(", ");
        first = false;
        ScriptVariant *var = storage.getPtr(i);
        if (var->vt == VT_OBJECT) ObjectHeap_GetObject(var->objVal)->print();
        else if (var->vt == VT_LIST) ObjectHeap_GetList(var->objVal)->print();
        else
        {
            ScriptVariant_ToString(var, buf, sizeof(buf));
            printf("%s", buf);
        }
    }
    printf("]");
    currentlyPrinting = false;
}

int ScriptList::toString(char *dst, int dstsize)
{
#define SNPRINTF(...) { int n = snprintf(dst, dstsize, __VA_ARGS__); dst += n; length += n; dstsize -= n; if (dstsize < 0) dstsize = 0; }
    char buf[256];
    bool first = true;
    int length = 0;

    if (currentlyPrinting)
    {
        SNPRINTF("[(list cycle)]");
        return length;
    }
    currentlyPrinting = true;

    SNPRINTF("[");
    for (size_t i = 0; i < storage.size(); i++)
    {
        if (!first) SNPRINTF(", ");
        first = false;
        ScriptVariant_ToString(storage.getPtr(i), buf, sizeof(buf));
        SNPRINTF("%s", buf);
    }
    SNPRINTF("]");
    currentlyPrinting = false;
    return length;
#undef SNPRINTF
}


