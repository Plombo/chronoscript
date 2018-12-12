#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "StrCache.hpp"

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

typedef struct {
    int len;
    int ref;
    char *str;
} Varstr;

#define STRCACHE_INC      64

class StrCache {
private:
    Varstr *strcache;
    int strcache_size;
    int strcache_top;
    int *strcache_index;

public:
    StrCache();

    // init the string cache
    void init();

    //clear the string cache
    void clear();

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

    void copy(int index, const char *str);
    void ncopy(int index, const char *str, int n);
    
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
    strcache = (Varstr*) calloc(STRCACHE_INC, sizeof(*strcache));
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
    //assert(strcache[index].ref>=0);
    if (!strcache[index].ref)
    {
        free(strcache[index].str);
        strcache[index].str = NULL;
        //if (strcache[index].len > MAX_STR_VAR_LEN)
        //	this->resize(index, MAX_STR_VAR_LEN);
        //assert(strcache_top+1<strcache_size);
        strcache_index[++strcache_top] = index;
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
        __reallocto(strcache, Varstr*, strcache_size, strcache_size + STRCACHE_INC);
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
    strcache[i].ref = 1;
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

void StrCache::copy(int index, const char *str)
{
    //assert(index<strcache_size);
    //assert(size>0);
    int len = strlen(str);
    if (strcache[index].len < len)
    {
        this->resize(index, len);
    }
    strcpy(strcache[index].str, str);
}

void StrCache::ncopy(int index, const char *str, int n)
{
    //assert(index<strcache_size);
    //assert(size>0);
    if (strcache[index].len < n)
    {
        this->resize(index, n);
    }
    strncpy(strcache[index].str, str, n);
    strcache[index].str[n] = 0;
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

static StrCache persistentCache; // global index = index (global index positive)
static StrCache temporaryCache; // global index = ~index (global index negative)

void StrCache_ClearTemporary()
{
    temporaryCache.clear();
}

// clear both string caches
void StrCache_ClearAll()
{
    persistentCache.clear();
    temporaryCache.clear();
}

// strings in the temporary cache will be dealt with by clear
void StrCache_Unref(int index)
{
    if (index >= 0)
        persistentCache.unref(index);
}

int StrCache_Pop(int length)
{
    return ~(temporaryCache.pop(length));
}

int StrCache_PopPersistent(int length)
{
    return persistentCache.pop(length);
}

void StrCache_Copy(int index, const char *str)
{
    if (index < 0)
        temporaryCache.copy(~index, str);
    else
        persistentCache.copy(index, str);
}

void StrCache_NCopy(int index, const char *str, int n)
{
    if (index < 0)
        temporaryCache.ncopy(~index, str, n);
    else
        persistentCache.ncopy(index, str, n);
}

char *StrCache_Get(int index)
{
    return index < 0 ? temporaryCache.get(~index) : persistentCache.get(index);
}

int StrCache_Len(int index)
{
    return index < 0 ? temporaryCache.len(~index) : persistentCache.len(index);
}

int StrCache_Ref(int index)
{
    if (index < 0)
    {
        // get a constant reference for this temporary string
        int newIndex = persistentCache.pop(temporaryCache.len(~index));
        persistentCache.copy(newIndex, temporaryCache.get(~index));
        return newIndex;
    }
    else
    {
        persistentCache.ref(index);
        return index;
    }
}

// see if a string is already in the persistent cache
// return its index if it is, or -1 if it isn't
int StrCache_FindString(const char *str)
{
    return persistentCache.findString(str);
}

}; // extern "C"

