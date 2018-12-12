#ifndef STRCACHE_HPP
#define STRCACHE_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include "depends.h"

//clear the string cache
void StrCache_ClearTemporary();
void StrCache_ClearAll();
void StrCache_Unref(int index);
int StrCache_Ref(int index);
int StrCache_Pop(int length);
int StrCache_PopPersistent(int length);
char *StrCache_Get(int index);
int StrCache_Len(int index);
void StrCache_Copy(int index, const char *str);
void StrCache_NCopy(int index, const char *str, int n);
int StrCache_FindString(const char *str);

#ifdef __cplusplus
};
#endif

#endif
