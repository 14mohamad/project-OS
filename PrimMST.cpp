// PrimMST.cpp
#include "MSTSolver.h"
#include <queue>
#include <functional>
#include <climits>  // Include this to use INT_MAX

class PrimMST : public MSTSolver {
public:
    Tree solveMST(const Graph& graph) override {
        cout << "Solving MST using Prim's Algorithm" << endl;
        int vertices = graph.getVertices();
        Tree mst(vertices);

        // Construct adjacency list from graph edges
        vector<vector<pair<int, int>>> adj(vertices);
        for (auto &edge : graph.getEdges()) {
            int w = edge.first, u = edge.second.first, v = edge.second.second;
            adj[u].push_back({w, v});
            adj[v].push_back({w, u});
        }

        // Priority queue to pick the smallest edge
        priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;
        vector<bool> inMST(vertices, false);
        vector<int> parent(vertices, -1); // Track parent nodes to print the MST
        vector<int> minWeight(vertices, INT_MAX); // Store minimum weight to each vertex

        int start = 0;
        pq.push({0, start}); // Start from vertex 0 with a weight of 0
        minWeight[start] = 0;

        while (!pq.empty()) {
            int u = pq.top().second;
            pq.pop();

            if (inMST[u]) continue; // Skip if already part of the MST

            inMST[u] = true; // Mark the vertex as included in the MST

            // If it's not the starting vertex, add the edge to the MST
            if (parent[u] != -1) {
                mst.addEdge(parent[u], u, minWeight[u]);
            }

            // Explore adjacent vertices and add to the priority queue
            for (auto &[weight, v] : adj[u]) {
                if (!inMST[v] && weight < minWeight[v]) {
                    minWeight[v] = weight;
                    pq.push({weight, v});
                    parent[v] = u; // Set u as the parent of v
                }
            }
        }

        return mst;
    }
};
