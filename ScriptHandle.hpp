#ifndef SCRIPT_HANDLE_HPP
#define SCRIPT_HANDLE_HPP

// Enumeration of every internal engine type accessible from script.
enum ScriptHandleType {
    SH_TYPE_INVALID = 0,
    SH_TYPE_ENTITY = 1,
    SH_TYPE_MODEL = 2,
    SH_TYPE_ANIMATION = 3,
};

// Every type in the engine must inherit from this class in order to be accessible as a script handle.
class ScriptHandle {
public:
    ScriptHandleType scriptHandleType;
};


#endif

