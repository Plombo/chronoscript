#ifndef SCRIPT_LIST_HPP
#define SCRIPT_LIST_HPP

#include "ScriptVariant.h"
#include <bits/stdc++.h> 

class ScriptList {
    friend class ObjectHeap;

private:
    std::vector<ScriptVariant> storage;
    bool persistent;
    bool currentlyPrinting;

public:
    ScriptList(size_t initialSize);
    ~ScriptList();

    inline bool get(ScriptVariant *dst, size_t index)
    {
        if (index >= storage.size())
            return false;

        *dst = storage.at(index);
        return true;
    }

    // don't call this directly; use ObjectHeap_SetListMember() instead
    inline bool set(size_t index, ScriptVariant value)
    {
        if (index >= storage.size())
            return false;

        ScriptVariant_Unref(&storage.at(index));
        storage.at(index) = value;
        return true;
    }

    inline size_t size()
    {
        return storage.size();
    }

    void makePersistent(); // make all values in list persistent
    void print();
    int toString(char *dst, int dstsize);
};

#endif

