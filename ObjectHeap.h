#ifndef OBJECT_HEAP_H
#define OBJECT_HEAP_H

#include "List.h"
#include "ScriptVariant.h"

class ScriptObject {
private:
    bool persistent;
    CList<ScriptVariant> map;

public:
    ScriptObject();
    ~ScriptObject();

    ScriptVariant get(const char *key);
    void set(const char *key, ScriptVariant value);
    void makePersistent(); // make all values in map persistent
};

// public API
void ObjectHeap_ClearTemporary();
void ObjectHeap_ClearAll();
int ObjectHeap_CreateNewObject();
void ObjectHeap_Ref(int index);
void ObjectHeap_Unref(int index);
ScriptObject *ObjectHeap_Get(int index);
int ObjectHeap_GetPersistentRef(int index);

#endif

