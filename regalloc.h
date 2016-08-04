#ifndef REGALLOC_H
#define REGALLOC_H

#include "liveness.h"

/**
 * Does register allocation by coloring an interference graph.
 */
class RegAlloc {
public:
    // vertices: the interference graph to color
    // vertexCount: number of vertices in the interference graph
    inline RegAlloc(InterferenceNode **vertices, int vertexCount)
        : vertices(vertices), vertexCount(vertexCount), ordering(NULL) {}
    ~RegAlloc();
    void run(); // run the register allocator
private:
    void maximumCardinalitySearch();
    void greedyColoring();

    InterferenceNode **vertices;
    InterferenceNode **ordering;
    int vertexCount;
};

#endif
