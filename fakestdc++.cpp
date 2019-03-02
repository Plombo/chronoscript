#include <stdlib.h>
#include "globals.h"

// MSVC uses the __cdecl calling convention for new and delete
#ifdef _MSC_VER
#define CDECL __cdecl
#else
#define CDECL
#endif

extern "C" void __cxa_pure_virtual()
{
    assert(!"pure virtual called");
}

void * CDECL operator new(size_t size)
{
    return malloc(size);
}

void * CDECL operator new[](size_t size)
{
    return malloc(size);
}

void CDECL operator delete(void *ptr)
{
    free(ptr);
}

void CDECL operator delete[](void *ptr)
{
    free(ptr);
}

