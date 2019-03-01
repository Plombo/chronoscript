#ifndef STRCACHE_HPP
#define STRCACHE_HPP

#ifdef __cplusplus
extern "C" {
#endif

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
int StrCache_PopPersistent(int length);
char *StrCache_Get(int index);
int StrCache_Len(int index);
const StrCacheEntry *StrCache_GetEntry(int index);
void StrCache_SetHash(int index, uint32_t hash);
void StrCache_Copy(int index, const char *str);
void StrCache_NCopy(int index, const char *str, int n);
int StrCache_FindString(const char *str);

#ifdef __cplusplus
};
#endif

#endif
