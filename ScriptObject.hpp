#ifndef SCRIPT_OBJECT_HPP
#define SCRIPT_OBJECT_HPP

#include "ScriptContainer.hpp"
#include "List.hpp"
#include "ScriptVariant.hpp"

class ScriptObject : public ScriptContainer {
    friend class ObjectHeap;
    friend void ObjectHeap_SetObjectMember(int, const char *, const ScriptVariant *);

private:
    List<ScriptVariant> map;
    bool currentlyPrinting;

    // don't call this directly; use ObjectHeap_SetObjectMember() instead
    void set(const char *key, const ScriptVariant *value);
    //void set(const ScriptVariant *key, const ScriptVariant *value);

public:
    ScriptObject();
    ~ScriptObject();

    // returns true on success, false on error
    bool get(ScriptVariant *dst, const ScriptVariant *key);

    void makePersistent() override; // make all values in map persistent
    void print() override;
    int toString(char *dst, int dstsize) override;
};

#endif

