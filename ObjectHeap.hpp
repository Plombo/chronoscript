#ifndef OBJECT_HEAP_HPP
#define OBJECT_HEAP_HPP

#include "ScriptObject.h"
#include "Stack.h"
#include "List.h"
#include "ScriptVariant.h"

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
int ObjectHeap_Ref(int index);
void ObjectHeap_Unref(int index);
ScriptObject *ObjectHeap_Get(int index);
void ObjectHeap_SetObjectMember(int index, const char *key, ScriptVariant *value);
void ObjectHeap_ListUnfreed();

// turn a white or black object gray (for garbage collection)
void GarbageCollector_PushGray(int index);

// process the entire gray stack, marking all objects black or white
void GarbageCollector_MarkAll();

// delete objects with no outstanding references (call only when all objects are marked)
void GarbageCollector_Sweep();

#endif

