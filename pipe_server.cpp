#include <iostream>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <fcntl.h>          // For non-blocking sockets
#include <sys/select.h>     // For select()
#include <mutex>            // For mutex locking
#include "Graph.h"
#include "MSTFactory.cpp"
#include "TaskQueue.h"
#include "PipelineWorker.h"

using namespace std;

Graph* currentGraph = nullptr;
atomic<bool> running(true);  // Atomic flag to indicate whether the server is running
mutex graph_mutex;           // Mutex to protect access to currentGraph
mutex coutMutex;             // Mutex to ensure thread-safe logging

// Thread-safe logging function
void logMessage(const string& message) {
    lock_guard<mutex> lock(coutMutex);
    cout << message << endl;
}

// Task Queues for each stage
TaskQueue requestQueue;    // Stage 1: Parse and validate requests
TaskQueue processingQueue; // Stage 2: Perform MST calculations
TaskQueue responseQueue;   // Stage 3: Send response to the client

// Function to handle the 'Newgraph' command
void handleNewGraph(int vertices, int client_sock) {
    lock_guard<mutex> lock(graph_mutex);  // Lock the mutex to protect currentGraph

    if (currentGraph != nullptr) {
        delete currentGraph; // Clean up any existing graph
    }

    currentGraph = new Graph(vertices);
    string response = "Graph initialized with " + to_string(vertices) + " vertices.\n";
    logMessage("[Server] Newgraph command processed.");

    // Move to response queue
    responseQueue.enqueue(Task(client_sock, response, [client_sock, response]() {
        send(client_sock, response.c_str(), response.size(), 0);
        logMessage("[Server] Response sent for Newgraph.");
    }));
}

// Function to handle the 'Newedge' command
void handleNewEdge(int u, int v, int weight, int client_sock) {
    lock_guard<mutex> lock(graph_mutex);  // Lock the mutex to protect currentGraph

    if (currentGraph == nullptr) {
        string response = "No graph initialized.\n";
        responseQueue.enqueue(Task(client_sock, response, [client_sock, response]() {
            send(client_sock, response.c_str(), response.size(), 0);
            logMessage("[Server] Response sent for invalid Newedge (no graph initialized).");
        }));
        return;
    }

    currentGraph->addEdge(u, v, weight);
    string response = "Added edge: " + to_string(u) + " -> " + to_string(v) + " with weight " + to_string(weight) + ".\n";
    logMessage("[Server] Newedge command processed.");

    // Move to response queue
    responseQueue.enqueue(Task(client_sock, response, [client_sock, response]() {
        send(client_sock, response.c_str(), response.size(), 0);
        logMessage("[Server] Response sent for Newedge.");
    }));
}

// Function to handle the 'Removeedge' command
void handleRemoveEdge(int u, int v, int client_sock) {
    lock_guard<mutex> lock(graph_mutex);  // Lock the mutex to protect currentGraph

    if (currentGraph == nullptr) {
        string response = "No graph initialized.\n";
        responseQueue.enqueue(Task(client_sock, response, [client_sock, response]() {
            send(client_sock, response.c_str(), response.size(), 0);
            logMessage("[Server] Response sent for invalid Removeedge (no graph initialized).");
        }));
        return;
    }

    currentGraph->removeEdge(u, v);
    string response = "Removed edge: " + to_string(u) + " -> " + to_string(v) + ".\n";
    logMessage("[Server] Removeedge command processed.");

    // Move to response queue
    responseQueue.enqueue(Task(client_sock, response, [client_sock, response]() {
        send(client_sock, response.c_str(), response.size(), 0);
        logMessage("[Server] Response sent for Removeedge.");
    }));
}

// Function to handle the 'MST' command and additional MST feature commands
void handleMST(const string& algorithm, int client_sock) {
    lock_guard<mutex> lock(graph_mutex);  // Lock the mutex to protect currentGraph

    if (currentGraph == nullptr) {
        string response = "No graph initialized.\n";
        responseQueue.enqueue(Task(client_sock, response, [client_sock, response]() {
            send(client_sock, response.c_str(), response.size(), 0);
            logMessage("[Server] Response sent for invalid MST command (no graph initialized).");
        }));
        return;
    }

    MSTSolver* solver = MSTFactory::getMSTSolver(algorithm);
    if (solver == nullptr) {
        string response = "Invalid algorithm. Use 'Prim', 'Kruskal', 'Longest', 'Shortest <src> <dest>', or 'Average'.\n";
        responseQueue.enqueue(Task(client_sock, response, [client_sock, response]() {
            send(client_sock, response.c_str(), response.size(), 0);
            logMessage("[Server] Response sent for invalid MST command.");
        }));
        return;
    }

    Tree mst = solver->solveMST(*currentGraph);
    stringstream result;

    // Handle the different types of MST-related requests
    if (algorithm == "Longest") {
        int longestDistance = mst.getLongestDistance();
        result << "Longest distance in MST: " << longestDistance << "\n";
    } else if (algorithm.rfind("Shortest", 0) == 0) {
        int src, dest;
        if (sscanf(algorithm.c_str(), "Shortest %d %d", &src, &dest) == 2) {
            int shortestDistance = mst.getShortestDistance(src, dest);
            result << "Shortest distance between " << src << " and " << dest << " in MST: " << shortestDistance << "\n";
        } else {
            result << "Invalid parameters for Shortest command. Use: MST Shortest <src> <dest>.\n";
        }
    } else if (algorithm == "Average") {
        double avgDistance = mst.getAverageDistance();
        result << "Average distance between vertices in MST: " << avgDistance << "\n";
    } else {
        // Default behavior for Prim and Kruskal
        result << "MST using " << algorithm << ":\n";
        for (const auto& edge : mst.getEdges()) {
            result << edge.second.first << " -- " << edge.second.second << " == " << edge.first << "\n";
        }
        result << "Total cost of MST: " << mst.getMSTCost() << "\n";
    }

    string response = result.str();
    logMessage("[Server] MST command (" + algorithm + ") processed.");

    // Move to response queue
    responseQueue.enqueue(Task(client_sock, response, [client_sock, response]() {
        send(client_sock, response.c_str(), response.size(), 0);
        logMessage("[Server] Response sent for MST command.");
    }));

    delete solver; // Clean up solver
}

// Function to enqueue request parsing into the pipeline
void enqueueRequestParsing(int client_sock, const string& request) {
    logMessage("[Server] Enqueuing request parsing.");
    requestQueue.enqueue(Task(client_sock, request, [client_sock, request]() {
        stringstream ss(request);
        string command;
        ss >> command;

        if (command == "Newgraph") {
            int vertices;
            ss >> vertices;
            // Enqueue into processingQueue
            processingQueue.enqueue(Task(client_sock, command, [client_sock, vertices]() {
                handleNewGraph(vertices, client_sock);
            }));
        } else if (command == "Newedge") {
            int u, v, weight;
            ss >> u >> v >> weight;
            // Enqueue into processingQueue
            processingQueue.enqueue(Task(client_sock, command, [client_sock, u, v, weight]() {
                handleNewEdge(u, v, weight, client_sock);
            }));
        } else if (command == "Removeedge") {
            int u, v;
            ss >> u >> v;
            // Enqueue into processingQueue
            processingQueue.enqueue(Task(client_sock, command, [client_sock, u, v]() {
                handleRemoveEdge(u, v, client_sock);
            }));
        } else if (command == "MST") {
            string algorithm;
            ss >> algorithm;
            // Enqueue into processingQueue
            processingQueue.enqueue(Task(client_sock, command, [client_sock, algorithm]() {
                handleMST(algorithm, client_sock);
            }));
        } else if (command == "shutdown") {
            // Handle shutdown request
            string response = "Server is shutting down.\n";
            responseQueue.enqueue(Task(client_sock, response, [client_sock, response]() {
                send(client_sock, response.c_str(), response.size(), 0);
                logMessage("[Server] Server is shutting down.");
            }));
            running = false;  // Set the running flag to false

            // Stop all task queues to unblock worker threads
            requestQueue.stop();
            processingQueue.stop();
            responseQueue.stop();
        } else {
            string response = "Invalid command.\n";
            responseQueue.enqueue(Task(client_sock, response, [client_sock, response]() {
                send(client_sock, response.c_str(), response.size(), 0);
                logMessage("[Server] Response sent for invalid command.");
            }));
        }
    }));
}

void startServer(int port) {
    int server_fd, client_sock = -1;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Set server_fd to non-blocking
    int flags = fcntl(server_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        exit(EXIT_FAILURE);
    }
    if (fcntl(server_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        exit(EXIT_FAILURE);
    }

    // Allow address reuse
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
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

    logMessage("Server started on port " + to_string(port));

    // Start pipeline workers
    PipelineWorker requestWorker(requestQueue);       // Handles request parsing
    PipelineWorker processingWorker(processingQueue); // Handles MST calculations
    PipelineWorker responseWorker(responseQueue);     // Handles sending the result

    std::thread worker1(std::ref(requestWorker));     // Start request parsing worker
    std::thread worker2(std::ref(processingWorker));  // Start processing worker
    std::thread worker3(std::ref(responseWorker));    // Start response worker

    fd_set readfds;
    struct timeval timeout;

    while (running) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_fd = server_fd;

        if (client_sock != -1) {
            FD_SET(client_sock, &readfds);
            if (client_sock > max_fd) {
                max_fd = client_sock;
            }
        }

        // Set timeout (e.g., 1 second)
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

        if ((activity < 0) && (errno != EINTR)) {
            perror("select error");
            break;
        }

        if (!running) break;

        if (activity == 0) {
            // Timeout occurred, no activity
            continue;
        }

        if (FD_ISSET(server_fd, &readfds)) {
            // Accept new connections
            client_sock = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
            if (client_sock >= 0) {
                // Set client_sock to non-blocking
                int flags = fcntl(client_sock, F_GETFL, 0);
                if (flags == -1) {
                    perror("fcntl F_GETFL client_sock");
                    exit(EXIT_FAILURE);
                }
                if (fcntl(client_sock, F_SETFL, flags | O_NONBLOCK) == -1) {
                    perror("fcntl F_SETFL client_sock");
                    exit(EXIT_FAILURE);
                }
                logMessage("[Server] Client connected.");
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("Accept failed");
                break;
            }
        }

        if (client_sock != -1 && FD_ISSET(client_sock, &readfds)) {
            // Handle client data
            memset(buffer, 0, sizeof(buffer));
            int valread = read(client_sock, buffer, 1024);
            if (valread > 0) {
                std::string request(buffer, valread);
                logMessage("[Server] Received request: " + request);
                enqueueRequestParsing(client_sock, request);
            } else if (valread == 0 || (valread < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                // Client disconnected or read error
                logMessage("[Server] Client disconnected or read error.");
                close(client_sock);
                client_sock = -1;
            }
        }
    }

    // Close the client socket if it's open
    if (client_sock != -1) {
        close(client_sock);
    }

    // Close the server socket
    close(server_fd);
    logMessage("[Server] Server has shut down.");

    // Join worker threads
    worker1.join();
    worker2.join();
    worker3.join();
}

int main() {
    int port = 8080;
    startServer(port);
    return 0;
}
