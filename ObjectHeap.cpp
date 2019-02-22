#include <stdio.h>
#include <stdlib.h>
#include "globals.h"
#include "ScriptObject.hpp"
#include "ObjectHeap.hpp"
#include "ScriptList.hpp"
#include "ArrayList.hpp"

#define __reallocto(p, t, n, s) \
    p = (t)realloc((p), sizeof(*(p))*(s));\
    memset((p)+(n), 0, sizeof(*(p))*((s)-(n)));

#define HEAP_SIZE_INCREMENT   64

enum GCColor {
    GC_COLOR_WHITE,
    GC_COLOR_GRAY,
    GC_COLOR_BLACK
};

enum MemberType {
    MEMBER_FREE,
    MEMBER_OBJECT,
};

class HeapMember {
public:
    union {
        struct {
            union {
                ScriptObject *obj;
                ScriptList *list;
            };
            bool isList;
            int refcount;
            int gcColor;
        } object;
        struct {
            int target;
        } link;
    };
    MemberType type;
};

class ObjectHeap {
    friend class GarbageCollector;
    
private:
    HeapMember *objects;
    ArrayList<int> tempRefsList;
    Stack<int> grayStack;
    int size; // allocated size of heap (includes unused space, not equal to # of active objects!)
    int top; // top of free_indices stack, equal to (# of free positions - 1)
    int *free_indices; // stack of indices of free objects

    int pop();

public:
    ObjectHeap();

    // init the object heap
    void init();

    // clear the object heap
    void clear();

    // clear all temporary references to objects and free all non-persistent objects
    void clearTemporaryReferences();

    // create a new object with refcount 1 and return its index
    int popObject();

    // create a new list with refcount 1 and return its index
    int popList(size_t initialSize);

    // increments the reference count for an object
    void ref(int index);

    // decrements the reference count for an object
    void unref(int index);

    // adds a temporary reference for an object if there isn't one already
    void addTemporaryReference(int index);

    // returns true if the object at the given index is persistent
    inline bool isPersistent(int index)
    {
        if (objects[index].type != MEMBER_OBJECT)
        {
            return false;
        }
        else if (objects[index].object.isList)
        {
            return objects[index].object.list->persistent;
        }
        else
        {
            return objects[index].object.obj->persistent;
        }
    }

    // returns true if the given index is a list (not a link to a list)
    inline bool isList(int index)
    {
        return objects[index].type == MEMBER_OBJECT && objects[index].object.isList;
    }

    // get the object with this index
    ScriptObject *getObject(int index);

    // get the list with this index
    ScriptList *getList(int index);

    // get the garbage collection color of the object with this index
    inline int getGCColor(int index)
    {
        assert(objects[index].type == MEMBER_OBJECT);
        return objects[index].object.gcColor;
    }

    // add an object to the gray list (for GC)
    void pushGray(int index);

    // mark phase of garbage collection
    inline void processOneGraySub(const ScriptVariant *var);
    void processOneGray();
    void markAll();

    // delete all objects whose GC color is white
    void sweep();

    // list all unfreed objects
    void listUnfreed();
};

ObjectHeap::ObjectHeap()
{
    objects = NULL;
    size = 0;
    top = -1;
    free_indices = NULL;
}

// init the object heap
void ObjectHeap::init()
{
    int i;
    clear(); // just in case
    objects = (HeapMember*) calloc(HEAP_SIZE_INCREMENT, sizeof(*objects));
    free_indices = (int*) malloc(HEAP_SIZE_INCREMENT * sizeof(*free_indices));
    for (i = 0; i < HEAP_SIZE_INCREMENT; i++)
    {
        free_indices[i] = i;
    }
    size = HEAP_SIZE_INCREMENT;
    top = size - 1;
}

// empty the object heap, freeing all objects in it
void ObjectHeap::clear()
{
    int i;
    if (objects)
    {
        for (i = 0; i < size; i++)
        {
            if (objects[i].type == MEMBER_OBJECT)
            {
                if (objects[i].object.isList)
                {
                    delete objects[i].object.list;
                }
                else
                {
                    delete objects[i].object.obj;
                }
            }
        }
        free(objects);
        objects = NULL;
    }

    if (free_indices)
    {
        free(free_indices);
        free_indices = NULL;
    }
    size = 0;
    top = -1;
    tempRefsList.clear();
}

// remove all temporary references and free all non-persistent objects
void ObjectHeap::clearTemporaryReferences()
{
    size_t numTempRefs = tempRefsList.size();
    for (size_t i = 0; i < numTempRefs; i++)
    {
        int index = tempRefsList.get(i);
        assert(objects[index].type == MEMBER_OBJECT);
        if (objects[index].object.refcount == 0 || !isPersistent(index))
        {
            objects[index].type = MEMBER_FREE;
            if (objects[index].object.isList)
            {
                delete objects[index].object.list;
            }
            else
            {
                delete objects[index].object.obj;
            }
            free_indices[++top] = index;
        }
    }

    tempRefsList.clear();
}

// creates a new object and returns its index
int ObjectHeap::pop()
{
    int i;
    if (size == 0)
    {
        init();
    }
    // no free spaces for objects, so expand the heap
    if (top < 0)
    {
        __reallocto(objects, HeapMember*, size, size + HEAP_SIZE_INCREMENT);
        __reallocto(free_indices, int*, size, size + HEAP_SIZE_INCREMENT);
        for (i = 0; i < HEAP_SIZE_INCREMENT; i++)
        {
            objects[size + i].type = MEMBER_FREE;
            free_indices[i] = size + i;
        }

        size += HEAP_SIZE_INCREMENT;
        top += HEAP_SIZE_INCREMENT;

        //printf("debug: object heap %p resized to %d \n", this, size);
    }
    i = free_indices[top--];
    assert(objects[i].type == MEMBER_FREE);
    objects[i].type = MEMBER_OBJECT;
    objects[i].object.refcount = 0;
    tempRefsList.append(i);
    objects[i].object.gcColor = GC_COLOR_WHITE;
    return i;
}

int ObjectHeap::popObject()
{
    int index = pop();
    objects[index].object.obj = new ScriptObject;
    objects[index].object.isList = false;
    return index;
}

int ObjectHeap::popList(size_t initialSize)
{
    int index = pop();
    objects[index].object.list = new ScriptList(initialSize);
    objects[index].object.isList = true;
    return index;
}

void ObjectHeap::ref(int index)
{
    assert(index < size && objects[index].type == MEMBER_OBJECT);
    ++objects[index].object.refcount;
}

void ObjectHeap::unref(int index)
{
    assert(index < size);
    // unreffing a free object is possible during garbage collection
    if (objects[index].type == MEMBER_FREE) return;

    --objects[index].object.refcount;
    assert(objects[index].object.refcount >= 0);
}

ScriptObject *ObjectHeap::getObject(int index)
{
    assert(index < size);
    assert(objects[index].type == MEMBER_OBJECT);
    assert(!objects[index].object.isList);
    return objects[index].object.obj;
}

ScriptList *ObjectHeap::getList(int index)
{
    assert(index < size);
    assert(objects[index].type == MEMBER_OBJECT);
    assert(objects[index].object.isList);
    return objects[index].object.list;
}

void ObjectHeap::pushGray(int index)
{
    assert(index >= 0);
    assert(objects[index].object.gcColor != GC_COLOR_GRAY);
    objects[index].object.gcColor = GC_COLOR_GRAY;
    grayStack.push(index);
}

inline void ObjectHeap::processOneGraySub(const ScriptVariant *var)
{
    if (var->vt == VT_OBJECT || var->vt == VT_LIST)
    {
        int subIndex = var->objVal;
        if (objects[subIndex].object.gcColor == GC_COLOR_WHITE)
        {
            pushGray(subIndex);
        }
    }
}

// process one item from the gray stack
void ObjectHeap::processOneGray()
{
    int index = grayStack.top();
    grayStack.pop();
    assert(objects[index].type == MEMBER_OBJECT);
    if (objects[index].object.isList)
    {
        ScriptList *list = objects[index].object.list;
        for (size_t i = 0; i < list->size(); i++)
        {
            ScriptVariant var;
            list->get(&var, i);
            processOneGraySub(&var);
        }
    }
    else
    {
        ScriptObject *obj = objects[index].object.obj;
        foreach_list(obj->map, ScriptVariant, iter)
        {
            processOneGraySub(iter.valuePtr());
        }
    }
    objects[index].object.gcColor = GC_COLOR_BLACK;
}

// mark objects until gray stack is empty
void ObjectHeap::markAll()
{
    while (!grayStack.isEmpty())
    {
        processOneGray();
    }
}

// delete all objects whose GC color is white
void ObjectHeap::sweep()
{
    assert(grayStack.isEmpty());
    for (int i = 0; i < size; i++)
    {
        if (objects[i].type == MEMBER_OBJECT &&
            objects[i].object.gcColor == GC_COLOR_WHITE)
        {
            //printf("delete object %i (gc)\n", i);
            objects[i].type = MEMBER_FREE;
            if (objects[i].object.isList)
                delete objects[i].object.list;
            else
                delete objects[i].object.obj;
            free_indices[++top] = i;
        }
    }
}

// list all unfreed objects in heap with printf
void ObjectHeap::listUnfreed()
{
    char buf[256];
    int i;
    for (i = 0; i < size; i++)
    {
        if (objects[i].type == MEMBER_OBJECT)
        {
            if (objects[i].object.isList)
                objects[i].object.list->toString(buf, sizeof(buf));
            else
                objects[i].object.obj->toString(buf, sizeof(buf));
            printf("Unfreed object %i: %s\n", i, buf);
        }
    }
}

// ------------------ GLOBAL INSTANCES ------------------
static ObjectHeap theHeap;

// -------------------- PUBLIC API ----------------------
void ObjectHeap_ClearTemporary()
{
    theHeap.clearTemporaryReferences();
}

void ObjectHeap_ClearAll()
{
    theHeap.clear();
}

// returns global index of newly created object (non-persistent)
int ObjectHeap_CreateNewObject()
{
    return theHeap.popObject();
}

// returns global index of newly created list (non-persistent)
int ObjectHeap_CreateNewList(size_t initialSize)
{
    return theHeap.popList(initialSize);
}

// makes temporary object persistent, or refs object if it's already persistent
int ObjectHeap_Ref(int index)
{
    if (!theHeap.isPersistent(index))
    {
        if (theHeap.isList(index))
        {
            ScriptList *list = theHeap.getList(index);
            list->makePersistent();
        }
        else
        {
            ScriptObject *obj = theHeap.getObject(index);
            obj->makePersistent();
        }
    }

    theHeap.ref(index);
    return index;
}

void ObjectHeap_Unref(int index)
{
    theHeap.unref(index);
}

ScriptObject *ObjectHeap_GetObject(int index)
{
    return theHeap.getObject(index);
}

ScriptList *ObjectHeap_GetList(int index)
{
    return theHeap.getList(index);
}

void ObjectHeap_SetObjectMember(int index, const char *key, const ScriptVariant *value)
{
    ScriptObject *obj = theHeap.getObject(index);

    // assigning something as a member of a persistent object
    if (theHeap.isPersistent(index))
    {
        // this will make the value persistent if it isn't already
        value = ScriptVariant_Ref(value);

        // a black object can't contain a white value, so make the white value gray
        if ((value->vt == VT_OBJECT || value->vt == VT_LIST) &&
            theHeap.getGCColor(index) == GC_COLOR_BLACK &&
            theHeap.getGCColor(value->objVal) == GC_COLOR_WHITE)
        {
            theHeap.pushGray(value->objVal);
        }
    }

    obj->set(key, value);
}

void ObjectHeap_SetListMember(int index, size_t indexInList, const ScriptVariant *value)
{
    ScriptList *list = theHeap.getList(index);

    // assigning something as a member of a persistent list
    if (theHeap.isPersistent(index))
    {
        // this will make the value persistent if it isn't already
        value = ScriptVariant_Ref(value);

        // a black object can't contain a white value, so make the white value gray
        if ((value->vt == VT_OBJECT || value->vt == VT_LIST) &&
            theHeap.getGCColor(index) == GC_COLOR_BLACK &&
            theHeap.getGCColor(value->objVal) == GC_COLOR_WHITE)
        {
            theHeap.pushGray(value->objVal);
        }
    }

    list->set(indexInList, *value);
}

bool ObjectHeap_InsertInList(int index, size_t indexInList, const ScriptVariant *value)
{
    ScriptList *list = theHeap.getList(index);

    // assigning something as a member of a persistent list
    if (theHeap.isPersistent(index))
    {
        // this will make the value persistent if it isn't already
        value = ScriptVariant_Ref(value);

        // a black object can't contain a white value, so make the white value gray
        if ((value->vt == VT_OBJECT || value->vt == VT_LIST) &&
            theHeap.getGCColor(index) == GC_COLOR_BLACK &&
            theHeap.getGCColor(value->objVal) == GC_COLOR_WHITE)
        {
            theHeap.pushGray(value->objVal);
        }
    }

    return list->insert(indexInList, *value);
}

void ObjectHeap_ListUnfreed()
{
    theHeap.listUnfreed();
}

void GarbageCollector_Sweep()
{
    theHeap.sweep();
}

void GarbageCollector_PushGray(int index)
{
    theHeap.pushGray(index);
}

void GarbageCollector_MarkAll()
{
    theHeap.markAll();
}


