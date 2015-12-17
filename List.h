

#ifndef LIST_H
#define LIST_H

#ifdef __cplusplus
extern "C" {
#endif

// this switch enables the capability to create a hashmap for quick index lookups
// speeds up search for "contains" and "getindex" type of queries.
// it only consumes more ram if CreateIndices is actually called
// but it can be disabled to reduce binary size and mem usage. however the mentioned
// methods will be much slower.
#define USE_INDEX

// this switch enables the capability to create a hashmap for quick string searches
// speeds up search for "findByname" type of queries.
// it will always be used, so memory usage will be slightly increased.
#define USE_STRING_HASHES

#define UNIT_TEST
#ifndef UNIT_TEST
//#include "depends.h"
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ralloc.h"

//A macro to simplify iterating through all this lists.
#define FOREACH( x, y ) { \
	  size = List_GetSize(&x); \
	  List_Reset(&x); \
	  for (i = 0; i < size; i++){ \
		 y \
		 List_GotoNext(&x); \
	  } \
   }

#define PFOREACH( x, y ) {\
   size = List_GetSize(x); \
   List_Reset(x); \
   for (i = 0; i < size; i++){ \
		 y \
		 List_GotoNext(x); \
	  } \
   }

#define NAME(s) ((s==NULL)?NULL:(strcpy((char*)malloc(strlen(s)+1),s)))

typedef struct Node
{
    struct Node *prev;          //pointer to previous Node
    struct Node *next;          //pointer to next Node
    void *value;                //data stored in a Node
    char *name;                //optional name of the Node
} Node;

#ifdef USE_INDEX
typedef struct LIndex
{
    size_t size;
    size_t used;
    Node **nodes;
    ptrdiff_t *indices;
} LIndex;
#endif

#ifdef USE_STRING_HASHES
typedef struct Bucket
{
    size_t size;
    size_t used;
    Node **nodes;
} Bucket;
#endif

typedef struct List
{
    //Data members
    Node *first;
    Node *current;
    Node *last;
    void **solidlist;
    int index;
    int size;
#ifdef USE_INDEX
    LIndex **mindices;
#endif
#ifdef USE_STRING_HASHES
    Bucket **buckets;
#endif
#ifdef DEBUG
    int initdone;
#endif

} List;

void List_SetCurrent(List *list, Node *current);
void Node_Clear(Node *node);
void List_Init(List *list);
void List_Solidify(List *list);
int List_GetIndex(List *list);
void List_Copy(List *listdest, const List *listsrc);
void List_Clear(List *list);
void List_InsertBefore(List *list, void *e, const char *theName);
void List_InsertAfter(List *list, void *e, const char *theName);
void List_Remove(List *list);
int List_GotoNext(List *list);
int List_GotoPrevious(List *list);
int List_GotoLast(List *list);
int List_GotoFirst(List *list);
void *List_Retrieve(const List *list);
void *List_GetFirst(const List *list);
void *List_GetLast(const List *list);
void List_Update(List *list, void *e);
int List_Includes(List *list, void *e);
int List_FindByName(List *list, const char *name);
char *List_GetName(const List *list);
void List_Reset(List *list);
int List_GetSize(const List *list);
int List_GotoIndex(List *list, int index);

Node *List_GetNodeByName(List *list, const char *Name);
Node *List_GetNodeByValue(List *list, void *e);
Node *List_GetCurrentNode(List *list);
int List_GetNodeIndex(List *list, Node *node);
#ifdef USE_INDEX
void List_AddIndex(List *list, Node *node, size_t index);
void List_RemoveLastIndex(List *list);
void List_CreateIndices(List *list);
void List_FreeIndices(List *list);
unsigned char ptrhash(void *value); // need to export that as well for unittest.
#endif

#ifdef __cplusplus
}; // extern "C"
extern "C++" {

// only works on non-solid lists
template <typename T>
class ListIterator {
private:
    Node *current;
public:
    inline ListIterator(Node *node) : current(node) {}
    inline Node *node() { return current; }
    inline char *name() { return current->name; }
    inline T *value() { return static_cast<T*>(current->value); }
    inline bool isFinished() { return (current == NULL); }
    inline ListIterator<T> next() { return ListIterator<T>(current->next); }
    inline bool hasNext() { return current && current->next; }
};

// thin OOP wrapper around List, should have the same performance
template <typename T>
class CList
{
    DECLARE_RALLOC_CXX_OPERATORS(CList);
public:
    List list;
    inline CList() { List_Init(&list); }
    inline ~CList() { clear(); }
    inline void clear() { List_Clear(&list); }
    inline void copyFrom(const List *src) { List_Copy(&list, src); }
    inline void copyFrom(const CList<T> *src) { List_Copy(&list, &src->list); }
    inline void insertBefore(T *e, const char *theName) { List_InsertBefore(&list, e, theName); }
    inline void insertAfter(T *e, const char *theName = NULL) { List_InsertAfter(&list, e, theName); }
    inline void remove() { List_Remove(&list); }
    inline void gotoNext() { List_GotoNext(&list); }
    inline void gotoPrevious() { List_GotoPrevious(&list); }
    inline void gotoLast() { List_GotoLast(&list); }
    inline void gotoFirst() { List_GotoFirst(&list); }
    inline T *retrieve() { return static_cast<T*>(List_Retrieve(&list)); }
    inline void update(T *e) { List_Update(&list, e); }
    inline bool includes(T *e) { return List_Includes(&list, e); }
    inline bool findByName(const char *name) { return List_FindByName(&list, name); }
    inline char *getName() { return List_GetName(&list); }
    inline int size() { return List_GetSize(&list); }
    inline bool isEmpty() { return 0 == List_GetSize(&list); }
    inline Node *currentNode() { return List_GetCurrentNode(&list); }
    inline void setCurrent(Node *node) { List_SetCurrent(&list, node); }
    inline bool gotoIndex(int index) { return List_GotoIndex(&list, index); }
    inline ListIterator<T> iterator() { return ListIterator<T>(list.first); }
};

#define foreach_list(list, type, var) \
    for(ListIterator<type> var = list.iterator(); !var.isFinished(); var = var.next())

};
#endif

#endif

