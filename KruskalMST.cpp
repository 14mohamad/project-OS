// KruskalMST.cpp
#include "MSTSolver.h"
#include <algorithm>

class KruskalMST : public MSTSolver {
private:
    // Utility function to find the set of an element i (for union-find)
    int findParent(int i, vector<int> &parent) {
        if (i == parent[i]) return i;
        return parent[i] = findParent(parent[i], parent);  // path compression
    }

    // Utility function to unite two sets (union-find)
    void unionSets(int u, int v, vector<int> &parent, vector<int> &rank) {
        int pu = findParent(u, parent);
        int pv = findParent(v, parent);
        if (rank[pu] > rank[pv]) {
            parent[pv] = pu;
        } else if (rank[pu] < rank[pv]) {
            parent[pu] = pv;
        } else {
            parent[pu] = pv;
            rank[pv]++;
        }
    }

public:
    Tree solveMST(const Graph& graph) override {
        cout << "Solving MST using Kruskal's Algorithm" << endl;
        int vertices = graph.getVertices();
        Tree mst(vertices);

        // Sort edges by weight
        auto edges = graph.getEdges();
        sort(edges.begin(), edges.end());

        vector<int> parent(vertices), rank(vertices, 0);
        for (int i = 0; i < vertices; ++i) {
            parent[i] = i;
        }

        for (auto &edge : edges) {
            int weight = edge.first, u = edge.second.first, v = edge.second.second;
            int pu = findParent(u, parent), pv = findParent(v, parent);

            // If u and v are in different sets, include this edge
            if (pu != pv) {
                mst.addEdge(u, v, weight);
                unionSets(u, v, parent, rank);
            }
        }

        return mst;
    }
};
