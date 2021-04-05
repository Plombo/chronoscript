#ifndef STRCACHE_HPP
#define STRCACHE_HPP

#include <stdint.h>
#include "depends.h"

typedef struct {
    int len;
    int ref;
    char *str;
    uint32_t hash;
} StrCacheEntry;

//clear the string cache
void StrCache_ClearTemporary();
void StrCache_ClearAll();
void StrCache_Unref(int index);
void StrCache_Ref(int index);
int StrCache_Pop(int length);
char *StrCache_Get(int index);
int StrCache_Len(int index);
const StrCacheEntry *StrCache_GetEntry(int index);
void StrCache_SetHash(int index);
int StrCache_FindString(const char *str);

#endif
