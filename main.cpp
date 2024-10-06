#include "MSTFactory.cpp"

// Function to run MST and print the result
void runMST(const Graph& graph, const string& algorithm) {
    MSTSolver* solver = MSTFactory::getMSTSolver(algorithm);
    if (solver) {
        Tree mst = solver->solveMST(graph);
        mst.printTree();
        cout << "Total cost of MST: " << mst.getMSTCost() << endl;
        delete solver;
    } else {
        cout << "Invalid algorithm selected!" << endl;
    }
}

// Function to remove an edge and recalculate the MST
void testEdgeRemoval(Graph& graph, const string& algorithm, int u, int v) {
    cout << "\n-- Testing edge removal: (" << u << ", " << v << ") --" << endl;
    
    // Remove the specified edge
    graph.removeEdge(u, v);
    cout << "Removed edge: " << u << " -> " << v << endl;
    
    // Run MST again after edge removal
    runMST(graph, algorithm);
}

int main() {
    // Define the initial graph
    Graph graph(5);
    graph.addEdge(0, 1, 2);
    graph.addEdge(0, 2, 3);
    graph.addEdge(1, 2, 1);
    graph.addEdge(1, 3, 4);
    graph.addEdge(2, 4, 5);

    // Choose algorithm (Prim or Kruskal)
    string algorithm;
    cout << "Choose MST algorithm (Prim, Kruskal): ";
    cin >> algorithm;

    // Run MST for the original graph
    cout << "\nInitial MST calculation:" << endl;
    runMST(graph, algorithm);

    // Perform tests by removing edges and re-running the MST
    testEdgeRemoval(graph, algorithm, 1, 2);  // Remove edge (1, 2)
    testEdgeRemoval(graph, algorithm, 2, 4);  // Remove edge (2, 4)
    
    // Additional test: Remove another edge
    testEdgeRemoval(graph, algorithm, 0, 2);  // Remove edge (0, 2)

    return 0;
}
