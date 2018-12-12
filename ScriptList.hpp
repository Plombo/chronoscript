#ifndef SCRIPT_LIST_HPP
#define SCRIPT_LIST_HPP

#include "ScriptVariant.hpp"
#include "ArrayList.hpp"

class ScriptList {
    friend class ObjectHeap;
    friend void ObjectHeap_SetListMember(int, size_t, const ScriptVariant *);
    friend bool ObjectHeap_InsertInList(int, size_t, const ScriptVariant *);

private:
    ArrayList<ScriptVariant> storage;
    bool persistent;
    bool currentlyPrinting;

public:
    inline ScriptList(size_t initialSize) :
        storage(initialSize, {{.ptrVal = 0}, VT_EMPTY}),
        persistent(false),
        currentlyPrinting(false)
    {}

    ~ScriptList();

    inline bool get(ScriptVariant *dst, size_t index)
    {
        if (index >= storage.size())
            return false;

        *dst = *storage.getPtr(index);
        return true;
    }

    inline size_t size()
    {
        return storage.size();
    }

    void makePersistent(); // make all values in list persistent
    void print();
    int toString(char *dst, int dstsize);

private:
    // don't call this directly; use ObjectHeap_SetListMember() instead
    inline bool set(size_t index, const ScriptVariant &value)
    {
        if (index >= storage.size())
            return false;

        ScriptVariant_Unref(storage.getPtr(index));
        storage.set(index, value);
        return true;
    }

    // don't call this directly; use ObjectHeap_InsertInList() instead
    inline bool insert(size_t index, const ScriptVariant &value)
    {
        // note that index can be equal to size
        if (index > storage.size())
            return false;

        storage.insert(index, value);
        return true;
    }
};

#endif

