#ifndef REGALLOC_H
#define REGALLOC_H

#include "liveness.h"

InterferenceNode **MCS(InterferenceNode **vertices, int vertexCount);
void greedyColoring(InterferenceNode **ordering, int vertexCount);

#endif
