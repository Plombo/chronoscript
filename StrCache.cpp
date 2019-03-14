#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "globals.h"
#include "StrCache.hpp"
#include "ArrayList.hpp"
#include "stringhash.h"

/*
The string cache is intended to reduce memory usage; since not all variants are
strings, there's no need to give all of them an array.

We actually keep two string caches: one for persistent strings like constants,
and one for temporary strings created while execting a script. We clear the
temporary cache every time a script is done executing.
*/

#define __reallocto(p, t, n, s) \
    p = (t)realloc((p), sizeof(*(p))*(s));\
    memset((p)+(n), 0, sizeof(*(p))*((s)-(n)));

#define STRCACHE_INC      64

class StrCache {
private:
    StrCacheEntry *strcache;
    int strcache_size;
    int strcache_top;
    int *strcache_index;
    ArrayList<int> tempRefs; // list of indices i where the refcount of string i might be zero

public:
    StrCache();

    // init the string cache
    void init();

    //clear the string cache
    void clear();

    // frees all strings with a refcount of 0
    void clearTemporary();

    // reallocs a string in the cache to a new size
    void resize(int index, int size);

    // increments a string's reference count
    void ref(int index);

    // unrefs a string
    void unref(int index);

    // get an index for a new string
    int pop(int length);

    // get the string with this index
    char *get(int index);

    // get the length of the string at this index
    int len(int index);

    // get a pointer to the entry in the string cache
    inline const StrCacheEntry *getEntry(int index);

    // set the hash of the entry if it doesn't already have one
    inline void setHash(int index);
    
    int findString(const char *str);
};

StrCache::StrCache()
{
    strcache = NULL;
    strcache_size = 0;
    strcache_top = -1;
    strcache_index = NULL;
}

// init the string cache
void StrCache::init()
{
    int i;
    this->clear(); // just in case
    strcache = (StrCacheEntry*) calloc(STRCACHE_INC, sizeof(*strcache));
    strcache_index = (int*) calloc(STRCACHE_INC, sizeof(*strcache_index));
    for (i = 0; i < STRCACHE_INC; i++)
    {
        strcache[i].str = NULL;
        strcache_index[i] = i;
    }
    strcache_size = STRCACHE_INC;
    strcache_top = strcache_size - 1;
}

//clear the string cache
void StrCache::clear()
{
    int i;
    if (strcache)
    {
        for (i = 0; i < strcache_size; i++)
        {
            if (strcache[i].str)
            {
                free(strcache[i].str);
            }
            strcache[i].str = NULL;
        }
        free(strcache);
        strcache = NULL;
    }
    if (strcache_index)
    {
        free(strcache_index);
        strcache_index = NULL;
    }
    strcache_size = 0;
    strcache_top = -1;
}

// frees all strings with a refcount of 0
void StrCache::clearTemporary()
{
    int numTemps = tempRefs.size();
    for (int i = 0; i < numTemps; i++)
    {
        int index = tempRefs.get(i);

        // If strcache[index].str is NULL, it means the index was added twice to tempRefs. No harm done.
        if (strcache[index].ref == 0 && strcache[index].str != NULL)
        {
            free(strcache[index].str);
            strcache[index].str = NULL;
            strcache_index[++strcache_top] = index;
        }
    }

    tempRefs.clear();
}

// reallocs a string in the cache to a new size
void StrCache::resize(int index, int size)
{
    //assert(index<strcache_size);
    //assert(size>0);
    strcache[index].str = (char*) realloc(strcache[index].str, size + 1);
    strcache[index].str[size] = 0;
    strcache[index].len = size;
}

// increments a string's reference count
void StrCache::ref(int index)
{
    strcache[index].ref++;
}

// unrefs a string
void StrCache::unref(int index)
{
    strcache[index].ref--;
    //printf("unref \"%s\" -> %i\n", strcache[index].str, strcache[index].ref);
    assert(strcache[index].ref >= 0);
    if (strcache[index].ref == 0)
    {
        tempRefs.append(index);
    }
}

// get an index for a new string
int StrCache::pop(int length)
{
    int i;
    if (strcache_size == 0)
    {
        init();
    }
    if (strcache_top < 0) // realloc
    {
        __reallocto(strcache, StrCacheEntry*, strcache_size, strcache_size + STRCACHE_INC);
        __reallocto(strcache_index, int*, strcache_size, strcache_size + STRCACHE_INC);
        for (i = 0; i < STRCACHE_INC; i++)
        {
            strcache_index[i] = strcache_size + i;
            strcache[i + strcache_size].str = NULL;
        }

        //printf("debug: dumping string cache....\n");
        //for (i=0; i<strcache_size; i++)
        //	printf("\t\"%s\"  %d\n", strcache[i].str, strcache[i].ref);

        strcache_size += STRCACHE_INC;
        strcache_top += STRCACHE_INC;

        //printf("debug: string cache resized to %d \n", strcache_size);
    }
    i = strcache_index[strcache_top--];
    strcache[i].str = (char*) malloc(length + 1);
    strcache[i].len = length;
    strcache[i].ref = 0;
    strcache[i].hash = 0;
    tempRefs.append(i);
    return i;
}

// get the string with this index
char *StrCache::get(int index)
{
    //assert(index<strcache_size);
    return strcache[index].str;
}

int StrCache::len(int index)
{
    //assert(index<strcache_size);
    return strcache[index].len;
}

inline const StrCacheEntry *StrCache::getEntry(int index)
{
    return &strcache[index];
}

void StrCache::setHash(int index)
{
    if (strcache[index].hash == 0)
    {
        strcache[index].hash = string_hash(strcache[index].str, strcache[index].len);
    }
}

// see if a string is already in the cache
// return its index if it is, or -1 if it isn't
int StrCache::findString(const char *str)
{
    int i;
    for (i = 0; i < strcache_size; i++)
    {
        if (strcache[i].ref && strcmp(str, strcache[i].str) == 0)
        {
            return i;
        }
    }
    return -1;
}

extern "C" {

static StrCache theCache;

void StrCache_ClearTemporary()
{
    theCache.clearTemporary();
}

// clear both string caches
void StrCache_ClearAll()
{
    theCache.clear();
}

// strings in the temporary cache will be dealt with by clear
void StrCache_Unref(int index)
{
    theCache.unref(index);
}

int StrCache_Pop(int length)
{
    return theCache.pop(length);
}

int StrCache_PopPersistent(int length)
{
    int index = theCache.pop(length);
    theCache.ref(index);
    return index;
}

char *StrCache_Get(int index)
{
    return theCache.get(index);
}

int StrCache_Len(int index)
{
    return theCache.len(index);
}

const StrCacheEntry *StrCache_GetEntry(int index)
{
    return theCache.getEntry(index);
}

void StrCache_SetHash(int index)
{
    theCache.setHash(index);
}

// note: there's no need for this to return anything anymore
void StrCache_Ref(int index)
{
    theCache.ref(index);
}

// see if a string is already in the persistent cache
// return its index if it is, or -1 if it isn't
int StrCache_FindString(const char *str)
{
    return theCache.findString(str);
}

}; // extern "C"

