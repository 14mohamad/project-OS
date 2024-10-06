// MSTSolver.h
#ifndef MSTSOLVER_H
#define MSTSOLVER_H

#include "Graph.h"
#include "Tree.h"

// Abstract base class for MST algorithms
class MSTSolver {
public:
    virtual Tree solveMST(const Graph& graph) = 0;
    virtual ~MSTSolver() = default;
};

#endif
