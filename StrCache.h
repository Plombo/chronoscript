#ifndef STRCACHE_H
#define STRCACHE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "depends.h"

//clear the string cache
void StrCache_Clear();
void StrCache_Collect(int index);
void StrCache_Grab(int index);
int StrCache_Pop();
char *StrCache_Get(int index);
void StrCache_Copy(int index, char *str);
void StrCache_NCopy(int index, char *str, int n);
int StrCache_FindString(char *str);

void StrCache_SetExecuting(int executing);
int StrCache_MakePersistent(int index);

#ifdef __cplusplus
};
#endif

#endif
