#ifndef SCRIPT_CONTAINER_HPP
#define SCRIPT_CONTAINER_HPP

// A base class for ScriptObject and ScriptList.
class ScriptContainer {
protected:
    bool persistent;

public:
    inline ScriptContainer() : persistent(false) {}

    virtual ~ScriptContainer();
    virtual void makePersistent() = 0;
    virtual void print() = 0;
    virtual int toString(char *dst, int dstsize) = 0;

    inline bool isPersistent()
    {
        return persistent;
    }
};

#endif

