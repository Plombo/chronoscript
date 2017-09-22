#ifndef SCRIPT_OBJECT_H
#define SCRIPT_OBJECT_H

#include "List.h"
#include "ScriptVariant.h"

class ScriptObject {
private:
    bool persistent;
    List<ScriptVariant*> map;

public:
    ScriptObject();
    ~ScriptObject();

    ScriptVariant get(const char *key);
    void set(const char *key, ScriptVariant value);
    void makePersistent(); // make all values in map persistent
    void print();
    int toString(char *dst, int dstsize);
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

