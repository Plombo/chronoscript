#ifndef LIVENESS_H
#define LIVENESS_H

#include "ssa.h"
#include "ralloc.h"

void computeLiveSets(SSAFunction *func);
void computeLiveIntervals(SSAFunction *func);

class InterferenceNode {
    DECLARE_RALLOC_CXX_OPERATORS(InterferenceNode);
public:
    int id;
    Interval livei;
    InterferenceNode *parent;
    CList<InterferenceNode> interferesWith;
    
    // for MCS
    int weight;
    InterferenceNode *mcsPrev, *mcsNext;
    bool ordered;
    int color;
    
    inline InterferenceNode() : id(-1), parent(NULL), livei()
    {}
    inline InterferenceNode *root() {
        return parent ? parent->root() : this;
    };
    inline bool isRoot() {
        return parent == NULL;
    };
    bool mergeInto(InterferenceNode *dst);
    void addInterference(InterferenceNode *other);
};

class LivenessAnalyzer {
private:
    void addLiveRange(Temporary *value, BasicBlock *block, int end);

    // merge the interference nodes of two temporaries
    bool mergeNodes(Temporary *dst, Temporary *src);
public:
    LivenessAnalyzer(SSAFunction *func);
    ~LivenessAnalyzer();

    void computeLiveIntervals();
    void coalesce();
    void buildInterferenceGraph();

private:
    void *memCtx;
    CList<Temporary> *temporaries;
    CList<BasicBlock> *basicBlocks;
    CList<Instruction> *instructions;
public:
    InterferenceNode **nodeForTemp; // indexed by temporary id
    InterferenceNode **values; // indexed by node id
    int uniqueNodes; // number of *unique* interference nodes (not the size of the nodes array)
};

#endif