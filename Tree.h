#ifndef TREE_H
#define TREE_H

#include <iostream>
#include <vector>
#include <utility>
#include <queue>
#include <limits>
#include <numeric>

using namespace std;

// Tree class representing the Minimum Spanning Tree
class Tree {
private:
    int vertices;  // Number of vertices in the tree
    vector<pair<int, pair<int, int>>> mst_edges;  // List of MST edges (weight, (u, v))
    vector<vector<pair<int, int>>> adj_list;      // Adjacency list for the MST, storing neighbors and weights

    // Utility function for DFS to find the longest path in the MST
    // Parameters: node (current node), parent (previous node in DFS), visited (visited nodes)
    // Returns: pair of (distance, farthest vertex)
    pair<int, int> dfs(int node, int parent, vector<bool>& visited) const {
        visited[node] = true;
        pair<int, int> farthest = {0, node};  // Initial farthest distance is 0 from itself
        for (auto& neighbor : adj_list[node]) {
            int next_node = neighbor.first;  // Neighbor node
            int weight = neighbor.second;    // Weight of the edge to the neighbor
            if (next_node != parent) {       // Avoid going back to the parent node
                auto result = dfs(next_node, node, visited);  // Recursively DFS on the neighbor
                result.first += weight;  // Accumulate the distance
                if (result.first > farthest.first) {  // If a longer path is found, update farthest
                    farthest = result;
                }
            }
        }
        return farthest;
    }

public:
    // Constructor to initialize the Tree with a given number of vertices
    Tree(int v) : vertices(v), adj_list(v) {}

    // Add an edge to the MST, updating both the edge list and adjacency list
    void addEdge(int u, int v, int weight) {
        mst_edges.push_back({weight, {u, v}});  // Store the edge
        adj_list[u].push_back({v, weight});     // Update adjacency list for vertex u
        adj_list[v].push_back({u, weight});     // Update adjacency list for vertex v (undirected graph)
    }

    // Print all edges of the MST in the format u -- v == weight
    void printTree() const {
        cout << "Minimum Spanning Tree:" << endl;
        for (auto &edge : mst_edges) {
            cout << edge.second.first << " -- " << edge.second.second << " == " << edge.first << endl;
        }
    }

    // Get the total weight of the MST (sum of all edge weights)
    int getMSTCost() const {
        return accumulate(mst_edges.begin(), mst_edges.end(), 0, 
            [](int sum, const pair<int, pair<int, int>>& edge) { return sum + edge.first; });
    }

    // Return the list of MST edges
    const vector<pair<int, pair<int, int>>>& getEdges() const {
        return mst_edges;
    }

    // Find the longest distance between two vertices in the MST (diameter of the tree)
    int getLongestDistance() const {
        vector<bool> visited(vertices, false);
        // First DFS to find the farthest node from node 0
        auto first_farthest = dfs(0, -1, visited);

        // Second DFS from that farthest node to find the actual longest path in the tree
        fill(visited.begin(), visited.end(), false);
        auto longest_path = dfs(first_farthest.second, -1, visited);
        
        return longest_path.first;  // Return the length of the longest path (tree diameter)
    }

    // Find the shortest distance between two vertices using BFS
    // Parameters: src (starting vertex), dest (target vertex)
    // Returns: shortest distance between src and dest
    int getShortestDistance(int src, int dest) const {
        if (src == dest) return 0;  // Distance from a node to itself is always 0
        
        vector<int> distances(vertices, numeric_limits<int>::max());  // Initialize distances to infinity
        queue<int> q;  // Queue for BFS
        q.push(src);   // Start BFS from src
        distances[src] = 0;  // Distance to itself is 0

        while (!q.empty()) {
            int node = q.front();  // Current node in BFS
            q.pop();
            for (auto& neighbor : adj_list[node]) {
                int next_node = neighbor.first;  // Neighbor node
                int weight = neighbor.second;    // Weight of the edge to the neighbor
                if (distances[next_node] == numeric_limits<int>::max()) {  // If neighbor not visited
                    distances[next_node] = distances[node] + weight;  // Update distance
                    q.push(next_node);  // Push the neighbor to the queue
                }
            }
        }
        return distances[dest];  // Return the shortest distance to the destination
    }

    // Calculate the average distance between all pairs of vertices
    // Returns: average distance (double)
    double getAverageDistance() const {
        double total_distance = 0;
        int num_pairs = 0;

        // Iterate over all pairs of vertices and compute shortest distances
        for (int i = 0; i < vertices; ++i) {
            for (int j = i + 1; j < vertices; ++j) {
                int distance = getShortestDistance(i, j);  // Find shortest distance between i and j
                if (distance != numeric_limits<int>::max()) {  // If the distance is valid
                    total_distance += distance;  // Accumulate the total distance
                    num_pairs++;  // Increment the count of valid pairs
                }
            }
        }

        // Return the average distance, ensuring no division by zero
        return (num_pairs > 0) ? total_distance / num_pairs : 0.0;
    }
};

#endif
