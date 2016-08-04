/* This implementation of register allocation is based on the algorithms
 * described in the paper "Register Allocation via Coloring of Chordal Graphs"
 * by Fernando Magno Quintao Pereira and Jens Palsberg:
 * http://web.cs.ucla.edu/~palsberg/paper/aplas05.pdf
 */

#include <stdlib.h>
#include "regalloc.h"
#include "liveness.h"

// stores interference nodes in buckets according to their weight
class WeightBuckets {
private:
    InterferenceNode **buckets; // buckets[i] == pointer to first node with weight i
    int maxWeight;
public:
    WeightBuckets(int vertexCount);
    ~WeightBuckets();
    void removeValue(InterferenceNode *value);
    void insertValue(InterferenceNode *value);
    inline InterferenceNode *next() { return buckets[maxWeight]; }
};

WeightBuckets::WeightBuckets(int vertexCount)
{
    maxWeight = 0;
    buckets = (InterferenceNode**) malloc(vertexCount * sizeof(InterferenceNode*));
    memset(buckets, 0, vertexCount * sizeof(InterferenceNode*));
}

WeightBuckets::~WeightBuckets()
{
    free(buckets);
}

void WeightBuckets::removeValue(InterferenceNode *value)
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

void WeightBuckets::insertValue(InterferenceNode *value)
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

RegAlloc::~RegAlloc()
{
    free(ordering);
}

void RegAlloc::run()
{
    maximumCardinalitySearch();
    greedyColoring();
}

// generates a simplicial elimination ordering of interference vertices
void RegAlloc::maximumCardinalitySearch()
{
    WeightBuckets nodeBuckets(vertexCount);

    // initialize simplicial elimination ordering array
    ordering = (InterferenceNode**) malloc(vertexCount * sizeof(InterferenceNode*));

    // For all v ∈ V do λ(v) ← 0
    // we iterate backwards just so the register numbers come out slightly nicer
    for (int i = vertexCount - 1; i >= 0; i--)
    {
        InterferenceNode *v = vertices[i];
        v->mcsNext = v->mcsPrev = NULL;
        v->weight = 0;
        v->ordered = false;
        v->color = -1;
        nodeBuckets.insertValue(v);
    }

    for (int i = 0; i < vertexCount; i++)
    {
        // let v ∈ V be a vertex such that ∀u ∈ V, λ(v) ≥ λ(u)
        InterferenceNode *v = nodeBuckets.next();
        // σ(i) ← v
        ordering[i] = v;
        // For all u ∈ V ∩ N(v) do λ(u) ← λ(u) + 1
        foreach_list(v->interferesWith, InterferenceNode, iter)
        {
            InterferenceNode *u = iter.value();
            if (u->ordered) continue; // make sure u is in V
            nodeBuckets.removeValue(u);
            ++u->weight;
            nodeBuckets.insertValue(u);
        }
        // V ← V − {v}
        v->ordered = true;
        nodeBuckets.removeValue(v);
    }
}

// input: nodes in simplicial elimination ordering
// assigns a color to each node; the greedy algorithm produces
// an optimal coloring since the input is a simplicial elimination
// ordering
void RegAlloc::greedyColoring()
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


