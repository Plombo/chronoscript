#include <stdlib.h>
#include "ssa.h"
#include "regalloc.h"
#include "liveness.h"

static InterferenceNode **buckets; // buckets[i] == pointer to first temp with cardinality i
int maxWeight;

static void removeValue(InterferenceNode *value)
{
    if (buckets[value->weight] == value)
        buckets[value->weight] = value->mcsNext;
    if (value->mcsPrev)
        value->mcsPrev->mcsNext = value->mcsNext;
    if (value->mcsNext)
        value->mcsNext->mcsPrev = value->mcsPrev;
    value->mcsPrev = value->mcsNext = NULL;
    while(maxWeight > 0 && buckets[maxWeight] == NULL) --maxWeight;
}

static void insertValue(InterferenceNode *value)
{
    int w = value->weight;
    value->mcsPrev = NULL;
    value->mcsNext = buckets[w];
    if (buckets[w])
    {
        buckets[w]->mcsPrev = value;
    }
    buckets[w] = value;
    if (w > maxWeight) maxWeight = w;
}

InterferenceNode **MCS(InterferenceNode **vertices, int vertexCount)
{
    void *memCtx = ralloc_context(NULL); // FIXME: memory leak, put this in a class and free it!
    buckets = (InterferenceNode**)malloc(vertexCount * sizeof(InterferenceNode*));
    memset(buckets, 0, vertexCount * sizeof(InterferenceNode*));

    // For all v ∈ V do λ(v) ← 0
    // we already start with weight==0 for each node
#if 0
    foreach_list((*instructionList), Instruction, iter)
    {
        if (!iter.value()->isExpression()) continue;
        Temporary *value = static_cast<Expression*>(iter.value())->value();
        if (value->isDead()) continue;
        value->cardinality = 0;
        insertValue(value);
        numTemps++;
    }
#endif

    // initialize simplicial elimination ordering array
    InterferenceNode **ordering = ralloc_array(memCtx, InterferenceNode*, vertexCount);
    maxWeight = 0;

    // we iterate backwards just to make the register numbers come out nicer
    for (int i = vertexCount - 1; i >= 0; i--)
    {
        InterferenceNode *v = vertices[i];
        v->mcsNext = v->mcsPrev = NULL;
        v->weight = 0;
        v->ordered = false;
        v->color = -1;
        insertValue(v);
    }
    
    for (int i = 0; i < vertexCount; i++)
    {
        // let v ∈ V be a vertex such that ∀u ∈ V,λ(v) ≥ λ(u)
        InterferenceNode *v = buckets[maxWeight];
        // σ(i) ← v
        ordering[i] = v;
        // For all u ∈ V ∩ N(v) do λ(u) ← λ(u) + 1
        foreach_list(v->interferesWith, InterferenceNode, iter)
        {
            InterferenceNode *u = iter.value();
            if (u->ordered) continue; // make sure u ∈ V
            removeValue(u);
            ++u->weight;
            insertValue(u);
        }
        // V ← V − {v}
        v->ordered = true;
        removeValue(v);
    }
    
    free(buckets);
    return ordering;
}

// input: temps in simplicial elimination ordering
void greedyColoring(InterferenceNode **ordering, int vertexCount)
{
    BitSet inUse(vertexCount, false); // colors in use by a neighbor
    for (int i = 0; i < vertexCount; i++)
    {
        inUse.clrAll();
        foreach_list(ordering[i]->interferesWith, InterferenceNode, iter)
        {
            int color = iter.value()->color;
            if (color >= 0) inUse.set(color);
        }
        int color = 0;
        while (inUse.test(color)) ++color;
        ordering[i]->color = color;
    }
}


