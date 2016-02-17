#include "depends.h"
#include "ssa.h"

class InterferenceGraph {
    void addConflict(s16 a, s16 b);
    bool hasConflict(s16 a, s16 b) const;
};

// http://ssabook.gforge.inria.fr/latest/book.pdf
// Chapter 22: page 323

void buildInterferenceGraph(SSABuilder *func, InterferenceGraph *igraph)
{
    s16 *lastUse = new s16[func->maxTemp];
    CList<Temporary> localLive;
    foreach_list(func->basicBlockList, BasicBlock, iter)
    {
        BasicBlock *block = iter.value();
    }
}
