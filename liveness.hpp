#ifndef LIVENESS_HPP
#define LIVENESS_HPP

#include "SSABuilder.hpp"
#include "ralloc.h"

void computeLiveSets(SSABuilder *func);
void computeLiveIntervals(SSABuilder *func);

class InterferenceNode {
    DECLARE_RALLOC_CXX_OPERATORS(InterferenceNode);
public:
    int id;
    Interval livei;
    InterferenceNode *parent;
    List<InterferenceNode*> interferesWith;
    
    // for MCS
    int weight;
    InterferenceNode *mcsPrev, *mcsNext;
    bool ordered;
    int color;
    
    inline InterferenceNode() : id(-1), livei(), parent(NULL)
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
    LivenessAnalyzer(SSABuilder *func);
    ~LivenessAnalyzer();

    void computeLiveIntervals();
    void coalesce();
    void buildInterferenceGraph();

private:
    void *memCtx;
    List<Temporary*> *temporaries;
    List<BasicBlock*> *basicBlocks;
public:
    InterferenceNode **nodeForTemp; // indexed by temporary id
    InterferenceNode **values; // indexed by node id
    int uniqueNodes; // number of *unique* interference nodes (not the size of the nodes array)
};

#endif
