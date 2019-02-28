#ifndef OBJECT_HEAP_HPP
#define OBJECT_HEAP_HPP

#include "ScriptContainer.hpp"
#include "ScriptObject.hpp"
#include "ScriptList.hpp"
#include "Stack.hpp"
#include "List.hpp"
#include "ScriptVariant.hpp"

/**
 * All objects are reference counted. The only references counted are from persistent places like global variables or
 * other persistent objects/lists. Objects with refcount 0 are freed when ObjectHeap_ClearTemporary() is called, and
 * not before, since temporary registers can hold references to objects with refcount 0.
 */

// public API
void ObjectHeap_ClearTemporary();
void ObjectHeap_ClearAll();
int ObjectHeap_CreateNewObject();
int ObjectHeap_CreateNewList(size_t initialSize);
void ObjectHeap_Ref(int index);
void ObjectHeap_Unref(int index);
ScriptContainer *ObjectHeap_Get(int index);
ScriptObject *ObjectHeap_GetObject(int index);
ScriptList *ObjectHeap_GetList(int index);
bool ObjectHeap_SetObjectMember(int index, const ScriptVariant *key, const ScriptVariant *value);
void ObjectHeap_SetListMember(int index, uint32_t indexInList, const ScriptVariant *value);
bool ObjectHeap_InsertInList(int index, uint32_t indexInList, const ScriptVariant *value);
void ObjectHeap_ListUnfreed();

// turn a white or black object gray (for garbage collection)
void GarbageCollector_PushGray(int index);

// process the entire gray stack, marking all objects black or white
void GarbageCollector_MarkAll();

// delete objects with no outstanding references (call only when all objects are marked)
void GarbageCollector_Sweep();

#endif

