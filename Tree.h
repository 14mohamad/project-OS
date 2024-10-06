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
    int vertices;
    vector<pair<int, pair<int, int>>> mst_edges;  // (weight, (u, v))
    vector<vector<pair<int, int>>> adj_list;      // Adjacency list to store the tree

    // Utility function for DFS to find the longest path in the MST
    pair<int, int> dfs(int node, int parent, vector<bool>& visited) const {
        visited[node] = true;
        pair<int, int> farthest = {0, node};  // {distance, vertex}
        for (auto& neighbor : adj_list[node]) {
            int next_node = neighbor.first;
            int weight = neighbor.second;
            if (next_node != parent) {
                auto result = dfs(next_node, node, visited);
                result.first += weight;
                if (result.first > farthest.first) {
                    farthest = result;
                }
            }
        }
        return farthest;
    }

public:
    Tree(int v) : vertices(v), adj_list(v) {}

    // Add edge to the MST
    void addEdge(int u, int v, int weight) {
        mst_edges.push_back({weight, {u, v}});
        adj_list[u].push_back({v, weight});
        adj_list[v].push_back({u, weight});  // Since it's an undirected tree
    }

    // Print the MST edges
    void printTree() const {
        cout << "Minimum Spanning Tree:" << endl;
        for (auto &edge : mst_edges) {
            cout << edge.second.first << " -- " << edge.second.second << " == " << edge.first << endl;
        }
    }

    // Return the total weight (cost) of the MST
    int getMSTCost() const {
        return accumulate(mst_edges.begin(), mst_edges.end(), 0, 
            [](int sum, const pair<int, pair<int, int>>& edge) { return sum + edge.first; });
    }

    // Return the MST edges
    const vector<pair<int, pair<int, int>>>& getEdges() const {
        return mst_edges;
    }

    // Find the longest distance between two vertices in the MST (Tree diameter)
    int getLongestDistance() const {
        vector<bool> visited(vertices, false);
        // First DFS to find the farthest node from node 0
        auto first_farthest = dfs(0, -1, visited);

        // Second DFS from that farthest node to find the actual longest path
        fill(visited.begin(), visited.end(), false);
        auto longest_path = dfs(first_farthest.second, -1, visited);
        
        return longest_path.first;  // Return the length of the longest path
    }

    // BFS to find shortest distance between any two vertices in MST
    int getShortestDistance(int src, int dest) const {
        if (src == dest) return 0;  // Distance to itself is 0
        
        vector<int> distances(vertices, numeric_limits<int>::max());
        queue<int> q;
        q.push(src);
        distances[src] = 0;

        while (!q.empty()) {
            int node = q.front();
            q.pop();
            for (auto& neighbor : adj_list[node]) {
                int next_node = neighbor.first;
                int weight = neighbor.second;
                if (distances[next_node] == numeric_limits<int>::max()) {  // If not visited
                    distances[next_node] = distances[node] + weight;
                    q.push(next_node);
                }
            }
        }
        return distances[dest];
    }

    // Calculate the average distance between all vertex pairs
    double getAverageDistance() const {
        double total_distance = 0;
        int num_pairs = 0;

        for (int i = 0; i < vertices; ++i) {
            for (int j = i + 1; j < vertices; ++j) {
                int distance = getShortestDistance(i, j);
                if (distance != numeric_limits<int>::max()) {
                    total_distance += distance;
                    num_pairs++;
                }
            }
        }

        return (num_pairs > 0) ? total_distance / num_pairs : 0.0;
    }
};

#endif
