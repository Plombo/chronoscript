#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "StrCache.h"

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
    void grab(int index);

    // unrefs a string
    void collect(int index);

    // get an index for a new string
    int pop(int length);

    // get the string with this index
    char *get(int index);

    // get the length of the string at this index
    int len(int index);

    void copy(int index, char *str);
    void ncopy(int index, char *str, int n);
    
    int findString(char *str);
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
        strcache[i].len = 0;
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
void StrCache::grab(int index)
{
    strcache[index].ref++;
}

// unrefs a string
void StrCache::collect(int index)
{
    strcache[index].ref--;
    //assert(strcache[index].ref>=0);
    if (!strcache[index].ref)
    {
        free(strcache[index].str);
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
            strcache[i + strcache_size].len = 0;
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

void StrCache::copy(int index, char *str)
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

void StrCache::ncopy(int index, char *str, int n)
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
int StrCache::findString(char *str)
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

static StrCache constantCache; // global index = index (global index positive)
static StrCache executionCache; // global index = ~index (global index negative)
static bool isExecuting = false;

void StrCache_SetExecuting(int executing)
{
    if (executing == isExecuting) return;
    isExecuting = executing;
    if (!executing) // stop execution; clear execution cache
    {
        executionCache.clear();
    }
}

//clear both string caches
void StrCache_Clear()
{
    constantCache.clear();
    executionCache.clear();
}

// only collect strings during compilation, not during execution
// any strings created during script execution will be dealt with by clear
void StrCache_Collect(int index)
{
    if (!isExecuting)
    {
        constantCache.collect(index);
    }
}

int StrCache_Pop(int length)
{
    return isExecuting ? ~(executionCache.pop(length)) : constantCache.pop(length);
}

void StrCache_Copy(int index, char *str)
{
    if (index < 0)
        executionCache.copy(~index, str);
    else
        constantCache.copy(index, str);
}

void StrCache_NCopy(int index, char *str, int n)
{
    if (index < 0)
        executionCache.ncopy(~index, str, n);
    else
        constantCache.ncopy(index, str, n);
}

char *StrCache_Get(int index)
{
    return index < 0 ? executionCache.get(~index) : constantCache.get(index);
}

int StrCache_Len(int index)
{
    return index < 0 ? executionCache.len(~index) : constantCache.len(index);
}

void StrCache_Grab(int index)
{
    if (index < 0)
        executionCache.grab(~index);
    else
        constantCache.grab(index);
}

// make a copy of the string in the constant cache if it isn't already there
int StrCache_MakePersistent(int index)
{
    if (index >= 0) return index;
    int newIndex = constantCache.pop(executionCache.len(~index));
    constantCache.copy(newIndex, executionCache.get(~index));
    return newIndex;
}

// see if a string is already in the cache
// return its index if it is, or -1 if it isn't
int StrCache_FindString(char *str)
{
    return constantCache.findString(str);
}

}; // extern "C"

