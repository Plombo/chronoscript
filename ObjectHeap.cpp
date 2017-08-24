#include "List.h"
#include "ScriptVariant.h"
#include "StrCache.h"
#include "ObjectHeap.h"

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
            ScriptObject *obj; // TODO allocate object as part of the structure
        } object;
        struct {
            int target;
        } link;
    };
};

ScriptObject::ScriptObject()
{
    persistent = false;
}

// destructor that destroys all pointers in map
ScriptObject::~ScriptObject()
{
    map.gotoFirst();
    while (map.size())
    {
        ScriptVariant *storage = map.retrieve();
        if (storage->vt == VT_STR)
        {
            StrCache_Collect(storage->strVal);
        }
        else if (storage->vt == VT_OBJECT)
        {
            ObjectHeap_Unref(storage->objVal);
        }
        delete storage;
        map.remove();
    }
}

void ScriptObject::set(const char *key, ScriptVariant value)
{
    if (persistent && value.vt == VT_STR)
    {
        if (value.strVal < 0)
        {
            value.strVal = StrCache_MakePersistent(value.strVal);
        }
        else
        {
            StrCache_Grab(value.strVal);
        }
    }
    else if (persistent && value.vt == VT_OBJECT)
    {
        value.objVal = ObjectHeap_GetPersistentRef(value.objVal);
    }

    if (map.findByName(key))
    {
        ScriptVariant *storage = map.retrieve();
        if (storage->vt == VT_STR)
        {
            StrCache_Collect(storage->strVal);
        }
        else if (storage->vt == VT_OBJECT)
        {
            ObjectHeap_Unref(storage->objVal);
        }
        *storage = value;
    }
    else
    {
        ScriptVariant *storage = new ScriptVariant;
        *storage = value;
        map.gotoLast();
        map.insertAfter(storage, key);
    }
}

ScriptVariant ScriptObject::get(const char *key)
{
    if (map.findByName(key))
    {
        return *map.retrieve();
    }
    else
    {
        ScriptVariant retvar = {{.ptrVal = NULL}, VT_EMPTY};
        return retvar;
    }
}

void ScriptObject::makePersistent()
{
    if (persistent) return;
    persistent = true; // set it up here to avoid infinite recursion in case of cycles
    foreach_list(map, ScriptVariant, iter)
    {
        ScriptVariant *var = iter.value();
        // FIXME need to ref object and string even if they are already persistent
        if (var->vt == VT_OBJECT && var->objVal < 0)
        {
            //printf("make object %i persistent\n", var->objVal);
            var->objVal = ObjectHeap_GetPersistentRef(var->objVal);
        }
        else if (var->vt == VT_STR && var->strVal < 0)
        {
            //printf("make string %i persistent\n", var->strVal);
            var->strVal = StrCache_MakePersistent(var->strVal);
        }
    }
}

void ScriptObject::print()
{
    char buf[256];
    printf("{");
    foreach_list(map, ScriptVariant, iter)
    {
        printf("\"%s\": ", iter.name());
        ScriptVariant *var = iter.value();
        if (var->vt == VT_OBJECT) ObjectHeap_Get(var->objVal)->print();
        else
        {
            ScriptVariant_ToString(var, buf);
            printf("%s", buf);
        }
        printf(", ");
    }
    printf("}");
}

void ScriptObject::toString(char *dst, int dstsize)
{
#define SNPRINTF(...) { int n = snprintf(dst, dstsize, __VA_ARGS__); dst += n; dstsize -= n; if (dstsize < 0) dstsize = 0; }
    char buf[256];
    SNPRINTF("{");
    foreach_list(map, ScriptVariant, iter)
    {
        SNPRINTF("\"%s\": ", iter.name());
        ScriptVariant *var = iter.value();
        ScriptVariant_ToString(var, buf);
        SNPRINTF("%s", buf);
        SNPRINTF(", ");
    }
    SNPRINTF("}");
#undef SNPRINTF
}

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

    // replaces object with link to global index
    void replaceWithLink(int index, int target);

    // get the object with this index
    ScriptObject *get(int index);
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
    if (objects[index].type == MEMBER_OBJECT)
    {
        assert(objects[index].object.refcount >= 1);
        --objects[index].object.refcount;
        if (objects[index].object.refcount == 0)
        {
            delete objects[index].object.obj;
            objects[index].type = MEMBER_FREE;
            //printf("delete object %i\n", index);
        }
    }
    else assert(objects[index].type == MEMBER_LINK);
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

void ObjectHeap_Ref(int index)
{
    if (index < 0)
        temporaryHeap.ref(~index);
    else
        persistentHeap.ref(index);
}

void ObjectHeap_Unref(int index)
{
    if (index < 0)
        temporaryHeap.unref(~index);
    else
        persistentHeap.unref(index);
}

ScriptObject *ObjectHeap_Get(int index)
{
    return (index < 0) ? temporaryHeap.get(~index) : persistentHeap.get(index);
}

// makes temporary object persistent, or refs object if it's already persistent
// FIXME handle case where index is already a link
int ObjectHeap_GetPersistentRef(int index)
{
    if (index >= 0)
    {
        persistentHeap.ref(index);
        return index;
    }
    else
    {
        // FIXME make this not infinite loop for cycles...
        ScriptObject *obj = temporaryHeap.get(~index);
        int newIndex = persistentHeap.steal(obj);
        temporaryHeap.replaceWithLink(~index, newIndex);
        obj->makePersistent();
        //printf("made persistent: %i -> %i\n", index, newIndex);
        return newIndex;
    }
}

