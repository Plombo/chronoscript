#include <stdio.h>
#include <stdlib.h>
#include "ScriptObject.h"

#define __reallocto(p, t, n, s) \
    p = (t)realloc((p), sizeof(*(p))*(s));\
    memset((p)+(n), 0, sizeof(*(p))*((s)-(n)));

#define HEAP_SIZE_INCREMENT   64

enum MemberType {
    MEMBER_FREE,
    MEMBER_OBJECT,
    MEMBER_LINK,
};

class HeapMember {
public:
    MemberType type;
    union {
        struct {
            int refcount;
            ScriptObject *obj;
        } object;
        struct {
            int target;
        } link;
    };
};

class ObjectHeap {
private:
    HeapMember *objects;
    int size; // allocated size of heap (includes unused space, not equal to # of active objects!)
    int top; // top of free_indices stack, equal to (# of free positions - 1)
    int *free_indices; // stack of indices of free objects

public:
    ObjectHeap();

    // init the object heap
    void init();

    // clear the object heap
    void clear();

    // create a new object with refcount 1 and return its index
    int pop();

    // increments the reference count for an object
    void ref(int index);

    // decrements the reference count for an object
    void unref(int index);

    // takes over an object from another heap
    int steal(ScriptObject *obj);

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
    ScriptObject *get(int index);

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
                delete objects[i].object.obj;
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
    objects[i].object.obj = new ScriptObject;
    objects[i].object.refcount = 1;
    return i;
}

void ObjectHeap::ref(int index)
{
    assert(index < size && objects[index].type == MEMBER_OBJECT);
    ++objects[index].object.refcount;
}

void ObjectHeap::unref(int index)
{
    assert(index < size);
    assert(objects[index].type == MEMBER_OBJECT);
    assert(objects[index].object.refcount >= 1);

    --objects[index].object.refcount;
    if (objects[index].object.refcount == 0)
    {
        delete objects[index].object.obj;
        objects[index].type = MEMBER_FREE;
        //printf("delete object %i\n", index);
    }
}

ScriptObject *ObjectHeap::get(int index)
{
    assert(index < size);
    if (objects[index].type == MEMBER_OBJECT)
    {
        return objects[index].object.obj;
    }
    else
    {
        assert(objects[index].type == MEMBER_LINK);
        return ObjectHeap_Get(objects[index].link.target);
    }
}

// takes over an object from another heap
int ObjectHeap::steal(ScriptObject *obj)
{
    int index = pop();
    assert(objects[index].type == MEMBER_OBJECT);
    delete objects[index].object.obj;
    objects[index].object.obj = obj;
    return index;
}

// replaces object with link to global index
void ObjectHeap::replaceWithLink(int index, int target)
{
    assert(index < size && objects[index].type == MEMBER_OBJECT);
    objects[index].type = MEMBER_LINK;
    objects[index].link.target = target;
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
            objects[i].object.obj->toString(buf, sizeof(buf));
            printf("Unfreed object %i: %s\n", i, buf);
        }
    }
}

// -------------------- PUBLIC API ----------------------
static ObjectHeap persistentHeap; // global index = index (global index positive)
static ObjectHeap temporaryHeap;  // global index = ~index (global index negative)

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
    return ~temporaryHeap.pop(); 
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
        ScriptObject *obj = temporaryHeap.get(~index);
        int newIndex = persistentHeap.steal(obj);
        temporaryHeap.replaceWithLink(~index, newIndex);
        obj->makePersistent();
        //printf("made persistent: %i -> %i\n", index, newIndex);
        return newIndex;
    }
}

void ObjectHeap_Unref(int index)
{
    if (index >= 0)
        persistentHeap.unref(index);
}

ScriptObject *ObjectHeap_Get(int index)
{
    return (index < 0) ? temporaryHeap.get(~index) : persistentHeap.get(index);
}

void ObjectHeap_ListUnfreed()
{
    persistentHeap.listUnfreed();
}


