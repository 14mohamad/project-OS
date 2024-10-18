#include <iostream>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "Graph.h"
#include "MSTFactory.cpp"
#include <cstring>

using namespace std;

Graph* currentGraph = nullptr;

// Function to handle the 'Newgraph' command
void handleNewGraph(int vertices, int client_sock) {
    if (currentGraph != nullptr) {
        delete currentGraph; // Clean up any existing graph
    }
    
    currentGraph = new Graph(vertices);
    string response = "Graph initialized with " + to_string(vertices) + " vertices.\n";
    send(client_sock, response.c_str(), response.size(), 0);
}

// Function to handle the 'Newedge' command
void handleNewEdge(int u, int v, int weight, int client_sock) {
    if (currentGraph == nullptr) {
        string response = "No graph initialized.\n";
        send(client_sock, response.c_str(), response.size(), 0);
        return;
    }

    currentGraph->addEdge(u, v, weight);
    string response = "Added edge: " + to_string(u) + " -> " + to_string(v) + " with weight " + to_string(weight) + ".\n";
    send(client_sock, response.c_str(), response.size(), 0);
}

// Function to handle the 'Removeedge' command
void handleRemoveEdge(int u, int v, int client_sock) {
    if (currentGraph == nullptr) {
        string response = "No graph initialized.\n";
        send(client_sock, response.c_str(), response.size(), 0);
        return;
    }

    currentGraph->removeEdge(u, v);
    string response = "Removed edge: " + to_string(u) + " -> " + to_string(v) + ".\n";
    send(client_sock, response.c_str(), response.size(), 0);
}

// Function to handle the 'MST' command
void handleMST(const string& algorithm, int client_sock) {
    if (currentGraph == nullptr) {
        string response = "No graph initialized.\n";
        send(client_sock, response.c_str(), response.size(), 0);
        return;
    }

    MSTSolver* solver = MSTFactory::getMSTSolver(algorithm);
    if (solver == nullptr) {
        string response = "Invalid algorithm. Use 'Prim' or 'Kruskal'.\n";
        send(client_sock, response.c_str(), response.size(), 0);
        return;
    }

    Tree mst = solver->solveMST(*currentGraph);
    stringstream result;
    
    result << "MST using " << algorithm << ":\n";
    for (const auto& edge : mst.getEdges()) {
        result << edge.second.first << " -- " << edge.second.second << " == " << edge.first << "\n";
    }
    result << "Total cost of MST: " << mst.getMSTCost() << "\n";

    string response = result.str();
    send(client_sock, response.c_str(), response.size(), 0);

    delete solver; // Clean up solver
}

// Server to listen for commands and process graph operations
void startServer(int port) {
    int server_fd, client_sock;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind the socket
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    cout << "Server started on port " << port << endl;

    while (true) {
        if ((client_sock = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        while (true) {
            memset(buffer, 0, sizeof(buffer)); // Clear buffer before each read
            int valread = read(client_sock, buffer, 1024);
            if (valread <= 0) {
                break; // Break out of the loop if the client disconnects
            }
            
            stringstream ss(buffer);
            string command;
            ss >> command;

            if (command == "Newgraph") {
                int vertices;
                ss >> vertices;
                handleNewGraph(vertices, client_sock);
            } else if (command == "Newedge") {
                int u, v, weight;
                ss >> u >> v >> weight;
                handleNewEdge(u, v, weight, client_sock);
            } else if (command == "Removeedge") {
                int u, v;
                ss >> u >> v;
                handleRemoveEdge(u, v, client_sock);
            } else if (command == "MST") {
                string algorithm;
                ss >> algorithm;
                handleMST(algorithm, client_sock);
            } else {
                string response = "Invalid command.\n";
                send(client_sock, response.c_str(), response.size(), 0);
            }
        }

        close(client_sock); // Close connection after processing
    }
}

int main() {
    int port = 8080;
    startServer(port);
    return 0;
}
