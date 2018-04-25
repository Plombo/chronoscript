#ifndef SCRIPT_OBJECT_H
#define SCRIPT_OBJECT_H

#include "List.h"
#include "ScriptVariant.h"

enum GCColor {
    GC_COLOR_WHITE,
    GC_COLOR_GRAY,
    GC_COLOR_BLACK
};

class ScriptObject {
private:
    bool persistent;
    bool currentlyPrinting;
    int gcColor;
    List<ScriptVariant> map;

public:
    ScriptObject();
    ~ScriptObject();

    ScriptVariant get(const char *key);
    void set(const char *key, ScriptVariant value);
    void makePersistent(); // make all values in map persistent
    void print();
    int toString(char *dst, int dstsize);
};

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
void ObjectHeap_ListUnfreed();

#endif

