#ifndef GRAPH_H
#define GRAPH_H

#include <iostream>
#include <vector>
#include <utility> // for std::pair
#include <algorithm> // for std::remove_if

using namespace std;

// Graph class to represent the graph
class Graph {
private:
    int vertices;
    vector<pair<int, pair<int, int>>> edges;  // (weight, (u, v)) format for Kruskal

public:
    Graph(int v) : vertices(v) {}

    bool addEdge(int u, int v, int weight) {
        // Check if edge already exists
        for (const auto& edge : edges) {
            if ((edge.second.first == u && edge.second.second == v) ||
                (edge.second.first == v && edge.second.second == u)) {
                return false; // Edge already exists
            }
        }
        edges.push_back({weight, {u, v}});
        return true; // Edge added successfully
    }

    int getVertices() const {
        return vertices;
    }

    const vector<pair<int, pair<int, int>>>& getEdges() const {
        return edges;
    }

    // Function to remove an edge (u, v)
    bool removeEdge(int u, int v) {
        auto it = remove_if(edges.begin(), edges.end(),
                            [u, v](const pair<int, pair<int, int>>& edge) {
                                return (edge.second.first == u && edge.second.second == v) ||
                                       (edge.second.first == v && edge.second.second == u);
                            });
        if (it != edges.end()) {
            edges.erase(it, edges.end());
            return true; // Edge removed successfully
        }
        return false; // Edge was not found
    }

    // Optional: Print the graph
    void printGraph() const {
        for (auto &edge : edges) {
            cout << edge.second.first << " -- " << edge.second.second << " == " << edge.first << endl;
        }
    }
};

#endif
