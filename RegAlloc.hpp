#ifndef REGALLOC_HPP
#define REGALLOC_HPP

#include "liveness.hpp"

/**
 * Does register allocation by coloring an interference graph.
 */
class RegAlloc {
public:
    // vertices: the interference graph to color
    // vertexCount: number of vertices in the interference graph
    inline RegAlloc(InterferenceNode **vertices, int vertexCount)
        : vertices(vertices), ordering(NULL), vertexCount(vertexCount) {}
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

