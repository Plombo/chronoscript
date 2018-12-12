#ifndef SCRIPT_OBJECT_HPP
#define SCRIPT_OBJECT_HPP

#include "List.hpp"
#include "ScriptVariant.hpp"

class ScriptObject {
    friend class ObjectHeap;

private:
    List<ScriptVariant> map;
    bool persistent;
    bool currentlyPrinting;

public:
    ScriptObject();
    ~ScriptObject();

    // returns true on success, false on error
    bool get(ScriptVariant *dst, const char *key);

    // don't call this directly; use ObjectHeap_SetObjectMember() instead
    void set(const char *key, ScriptVariant value);

    void makePersistent(); // make all values in map persistent
    void print();
    int toString(char *dst, int dstsize);
};

#endif

