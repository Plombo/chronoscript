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
        ScriptVariant_Ref(var);
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

int ScriptList::toString(char *dst, int dstsize, bool json)
{
#define SWRITE(expr)  { int n = (expr); dst += n; length += n; dstsize -= n; if (dstsize < 0) dstsize = 0; }
#define SNPRINTF(...) SWRITE(snprintf(dst, dstsize, __VA_ARGS__))
    bool first = true;
    int length = 0;

    if (currentlyPrinting)
    {
        SNPRINTF("[(list cycle)]");
        return length;
    }
    currentlyPrinting = true;

    SNPRINTF("[");
    for (uint32_t i = 0; i < storage.size(); i++)
    {
        if (!first) SNPRINTF(", ");
        first = false;

        const ScriptVariant *value = storage.getPtr(i);
        if (json || value->vt == VT_STR)
        {
            SWRITE(ScriptVariant_ToJSON(value, dst, dstsize));
        }
        else
        {
            SWRITE(ScriptVariant_ToString(value, dst, dstsize));
        }
    }
    SNPRINTF("]");
    currentlyPrinting = false;
    return length;
#undef SNPRINTF
#undef SWRITE
}


