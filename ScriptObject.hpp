#ifndef SCRIPT_OBJECT_HPP
#define SCRIPT_OBJECT_HPP

#include "ScriptContainer.hpp"
#include "List.hpp"
#include "ScriptVariant.hpp"

struct ObjectHashNode {
    int key; // index into string cache
    int next;
    ScriptVariant value;

    inline ObjectHashNode() : key(-1), next(-1) {}
};

class ScriptObject : public ScriptContainer {
    friend class ObjectHeap;
    friend bool ObjectHeap_SetObjectMember(int, const ScriptVariant *, const ScriptVariant *);

private:
    bool currentlyPrinting;
    ObjectHashNode *hashTable;
    unsigned int log2_hashTableSize;
    unsigned int lastFreeNode;

    ObjectHashNode *getNodeForKey(int key);
    ssize_t getFreePosition();
    inline size_t mainPositionForKey(int key);
    void resizeHashTable(unsigned int minNewSize);

    // don't call set() directly; use ObjectHeap_SetObjectMember() instead
    bool set(const ScriptVariant *key, const ScriptVariant *value);
    bool set(int key, const ScriptVariant *value);

public:
    ScriptObject(unsigned int initialSize);
    ~ScriptObject();

    // returns true on success, false on error
    bool get(ScriptVariant *dst, const ScriptVariant *key);

    void makePersistent() override; // make all values in map persistent
    void print() override;
    int toString(char *dst, int dstsize) override;
};

#endif

