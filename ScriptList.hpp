#ifndef SCRIPT_LIST_HPP
#define SCRIPT_LIST_HPP

#include <stdint.h>
#include "ScriptContainer.hpp"
#include "ScriptVariant.hpp"
#include "ArrayList.hpp"

class ScriptList : public ScriptContainer {
    friend class ObjectHeap;
    friend void ObjectHeap_SetListMember(int, uint32_t, const ScriptVariant *);
    friend bool ObjectHeap_InsertInList(int, uint32_t, const ScriptVariant *);

private:
    bool currentlyPrinting;
    ArrayList<ScriptVariant> storage;

public:
    inline ScriptList(uint32_t initialSize) :
        currentlyPrinting(false),
        storage(initialSize, {{.ptrVal = 0}, VT_EMPTY})
    {}

    ~ScriptList();

    inline bool get(ScriptVariant *dst, uint32_t index)
    {
        if (index >= storage.size())
            return false;

        *dst = *storage.getPtr(index);
        return true;
    }

    inline uint32_t size()
    {
        return storage.size();
    }

    inline bool remove(uint32_t index)
    {
        if (index >= storage.size())
            return false;

        if (persistent)
            ScriptVariant_Unref(storage.getPtr(index));
        storage.remove(index);
        return true;
    }

    void makePersistent() override; // make all values in list persistent
    void print() override;
    int toString(char *dst, int dstsize, bool json) override;

private:
    // don't call this directly; use ObjectHeap_SetListMember() instead
    inline bool set(uint32_t index, const ScriptVariant &value)
    {
        if (index >= storage.size())
            return false;

        if (persistent)
            ScriptVariant_Unref(storage.getPtr(index));
        storage.set(index, value);
        return true;
    }

    // don't call this directly; use ObjectHeap_InsertInList() instead
    inline bool insert(uint32_t index, const ScriptVariant &value)
    {
        // note that index can be equal to size
        if (index > storage.size())
            return false;

        storage.insert(index, value);
        return true;
    }
};

#endif

