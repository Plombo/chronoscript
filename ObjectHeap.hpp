#ifndef OBJECT_HEAP_HPP
#define OBJECT_HEAP_HPP

#include "ScriptObject.hpp"
#include "ScriptList.hpp"
#include "Stack.hpp"
#include "List.hpp"
#include "ScriptVariant.hpp"

/**
 * There are two object heaps: temporary and persistent. All objects are
 * put in the temporary heap when created with ObjectHeap_CreateNewObject(),
 * and can be moved to the persistent heap with ObjectHeap_MakePersistent().
 * The temporary heap is cleared after each call to Interpreter::runFunction(),
 * i.e., every time a "main" function finishes execution.
 */

// public API
void ObjectHeap_ClearTemporary();
void ObjectHeap_ClearAll();
int ObjectHeap_CreateNewObject();
int ObjectHeap_CreateNewList(size_t initialSize);
int ObjectHeap_Ref(int index);
void ObjectHeap_Unref(int index);
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

