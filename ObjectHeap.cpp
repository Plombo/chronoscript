#include <stdio.h>
#include <stdlib.h>
#include "ScriptObject.hpp"
#include "ObjectHeap.hpp"
#include "ScriptList.hpp"

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
    MEMBER_LINK,
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

    // create a new object with refcount 1 and return its index
    int popObject();

    // create a new list with refcount 1 and return its index
    int popList(size_t initialSize);

    // increments the reference count for an object
    void ref(int index);

    // decrements the reference count for an object
    void unref(int index);

    // takes over an object or list from another heap
    int stealObject(ScriptObject *obj);
    int stealList(ScriptList *list);

    // returns true if the given index is a list (not a link to a list)
    inline bool isList(int index)
    {
        return objects[index].type == MEMBER_OBJECT && objects[index].object.isList;
    }

    // returns true if the given index is a link
    inline bool isLink(int index)
    {
        return objects[index].type == MEMBER_LINK;
    }

    // gets (persistent) index that the given index links to
    inline int getLinkTarget(int index)
    {
        assert(objects[index].type == MEMBER_LINK);
        return objects[index].link.target;
    }

    // replaces object with link to global index
    void replaceWithLink(int index, int target);

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
    objects[i].object.refcount = 1;
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
    assert(objects[index].object.refcount >= 1);

    --objects[index].object.refcount;
    if (objects[index].object.refcount == 0)
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
        //printf("delete object %i\n", index);
    }
}

ScriptObject *ObjectHeap::getObject(int index)
{
    assert(index < size);
    if (objects[index].type == MEMBER_OBJECT)
    {
        assert(!objects[index].object.isList);
        return objects[index].object.obj;
    }
    else
    {
        assert(objects[index].type == MEMBER_LINK);
        return ObjectHeap_GetObject(objects[index].link.target);
    }
}

ScriptList *ObjectHeap::getList(int index)
{
    assert(index < size);
    if (objects[index].type == MEMBER_OBJECT)
    {
        assert(objects[index].object.isList);
        return objects[index].object.list;
    }
    else
    {
        assert(objects[index].type == MEMBER_LINK);
        return ObjectHeap_GetList(objects[index].link.target);
    }
}

// takes over an object from another heap
int ObjectHeap::stealObject(ScriptObject *obj)
{
    int index = pop();
    objects[index].object.obj = obj;
    objects[index].object.isList = false;
    return index;
}

// takes over a list from another heap
int ObjectHeap::stealList(ScriptList *list)
{
    int index = pop();
    objects[index].object.list = list;
    objects[index].object.isList = true;
    return index;
}

// replaces object with link to global index
void ObjectHeap::replaceWithLink(int index, int target)
{
    assert(index < size && objects[index].type == MEMBER_OBJECT);
    objects[index].type = MEMBER_LINK;
    objects[index].link.target = target;
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
static ObjectHeap persistentHeap; // global index = index (global index positive)
static ObjectHeap temporaryHeap;  // global index = ~index (global index negative)

// -------------------- PUBLIC API ----------------------
void ObjectHeap_ClearTemporary()
{
    temporaryHeap.clear();
}

void ObjectHeap_ClearAll()
{
    persistentHeap.clear();
    temporaryHeap.clear();
}

// returns global index of newly created object (non-persistent)
int ObjectHeap_CreateNewObject()
{
    return ~temporaryHeap.popObject(); 
}

// returns global index of newly created list (non-persistent)
int ObjectHeap_CreateNewList(size_t initialSize)
{
    return ~temporaryHeap.popList(initialSize); 
}

// makes temporary object persistent, or refs object if it's already persistent
int ObjectHeap_Ref(int index)
{
    if (index >= 0)
    {
        persistentHeap.ref(index);
        return index;
    }
    else if (temporaryHeap.isLink(~index))
    {
        int newIndex = temporaryHeap.getLinkTarget(~index);
        persistentHeap.ref(newIndex);
        return newIndex;
    }
    else
    {
        int newIndex;
        if (temporaryHeap.isList(~index))
        {
            ScriptList *list = temporaryHeap.getList(~index);
            newIndex = persistentHeap.stealList(list);
            temporaryHeap.replaceWithLink(~index, newIndex);
            list->makePersistent();
        }
        else
        {
            ScriptObject *obj = temporaryHeap.getObject(~index);
            newIndex = persistentHeap.stealObject(obj);
            temporaryHeap.replaceWithLink(~index, newIndex);
            obj->makePersistent();
        }
        //printf("made persistent: %i -> %i\n", index, newIndex);
        return newIndex;
    }
}

void ObjectHeap_Unref(int index)
{
    if (index >= 0)
        persistentHeap.unref(index);
}

ScriptObject *ObjectHeap_GetObject(int index)
{
    return (index < 0) ? temporaryHeap.getObject(~index) : persistentHeap.getObject(index);
}

ScriptList *ObjectHeap_GetList(int index)
{
    return (index < 0) ? temporaryHeap.getList(~index) : persistentHeap.getList(index);
}

static int followIfLink(int index)
{
    if (index < 0 && temporaryHeap.isLink(~index))
        return temporaryHeap.getLinkTarget(~index);
    else
        return index;
}

void ObjectHeap_SetObjectMember(int index, const char *key, const ScriptVariant *value)
{
    index = followIfLink(index); // we need to know if this is object is persistent
    ScriptObject *obj = ObjectHeap_GetObject(index);

    // assigning something as a member of a persistent object
    if (index >= 0)
    {
        // this will make the value persistent if it isn't already
        value = ScriptVariant_Ref(value);

        // a black object can't contain a white value, so make the white value gray
        if ((value->vt == VT_OBJECT || value->vt == VT_LIST) &&
            persistentHeap.getGCColor(index) == GC_COLOR_BLACK &&
            persistentHeap.getGCColor(value->objVal) == GC_COLOR_WHITE)
        {
            persistentHeap.pushGray(value->objVal);
        }
    }

    obj->set(key, value);
}

void ObjectHeap_SetListMember(int index, size_t indexInList, const ScriptVariant *value)
{
    index = followIfLink(index); // we need to know if this is object is persistent
    ScriptList *list = ObjectHeap_GetList(index);

    // assigning something as a member of a persistent list
    if (index >= 0)
    {
        // this will make the value persistent if it isn't already
        value = ScriptVariant_Ref(value);

        // a black object can't contain a white value, so make the white value gray
        if ((value->vt == VT_OBJECT || value->vt == VT_LIST) &&
            persistentHeap.getGCColor(index) == GC_COLOR_BLACK &&
            persistentHeap.getGCColor(value->objVal) == GC_COLOR_WHITE)
        {
            persistentHeap.pushGray(value->objVal);
        }
    }

    list->set(indexInList, *value);
}

bool ObjectHeap_InsertInList(int index, size_t indexInList, const ScriptVariant *value)
{
    index = followIfLink(index); // we need to know if this is object is persistent
    ScriptList *list = ObjectHeap_GetList(index);

    // assigning something as a member of a persistent list
    if (index >= 0)
    {
        // this will make the value persistent if it isn't already
        value = ScriptVariant_Ref(value);

        // a black object can't contain a white value, so make the white value gray
        if ((value->vt == VT_OBJECT || value->vt == VT_LIST) &&
            persistentHeap.getGCColor(index) == GC_COLOR_BLACK &&
            persistentHeap.getGCColor(value->objVal) == GC_COLOR_WHITE)
        {
            persistentHeap.pushGray(value->objVal);
        }
    }

    return list->insert(indexInList, *value);
}

void ObjectHeap_ListUnfreed()
{
    persistentHeap.listUnfreed();
}

void GarbageCollector_Sweep()
{
    persistentHeap.sweep();
}

void GarbageCollector_PushGray(int index)
{
    persistentHeap.pushGray(index);
}

void GarbageCollector_MarkAll()
{
    persistentHeap.markAll();
}


