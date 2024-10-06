// MSTFactory.cpp
#include "MSTSolver.h"
#include "PrimMST.cpp"
#include "KruskalMST.cpp"

class MSTFactory {
public:
    static MSTSolver* getMSTSolver(const string &algorithm) {
        if (algorithm == "Prim") {
            return new PrimMST();
        } else if (algorithm == "Kruskal") {
            return new KruskalMST();
        } else {
            return nullptr;
        }
    }
};
