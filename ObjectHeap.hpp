#ifndef OBJECT_HEAP_HPP
#define OBJECT_HEAP_HPP

#include "ScriptObject.hpp"
#include "ScriptList.hpp"
#include "Stack.hpp"
#include "List.hpp"
#include "ScriptVariant.hpp"

/**
 * All objects are reference counted. Any number of references in temporary registers counts as a single reference.
 * Otherwise, references are from persistent places like global variables or other objects/lists. Temporary references
 * are cleared every time Interpreter::runFunction() finishes executing a function call.
 */

// public API
void ObjectHeap_ClearTemporary();
void ObjectHeap_ClearAll();
int ObjectHeap_CreateNewObject();
int ObjectHeap_CreateNewList(size_t initialSize);
int ObjectHeap_Ref(int index);
void ObjectHeap_Unref(int index);
void ObjectHeap_AddTemporaryReference(int index);
ScriptObject *ObjectHeap_GetObject(int index);
ScriptList *ObjectHeap_GetList(int index);
void ObjectHeap_SetObjectMember(int index, const char *key, const ScriptVariant *value);
void ObjectHeap_SetListMember(int index, size_t indexInList, const ScriptVariant *value);
bool ObjectHeap_InsertInList(int index, size_t indexInList, const ScriptVariant *value);
void ObjectHeap_ListUnfreed();

// turn a white or black object gray (for garbage collection)
void GarbageCollector_PushGray(int index);

// process the entire gray stack, marking all objects black or white
void GarbageCollector_MarkAll();

// delete objects with no outstanding references (call only when all objects are marked)
void GarbageCollector_Sweep();

#endif

