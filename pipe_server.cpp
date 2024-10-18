// leader_follower_server.cpp

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
#include <vector>
#include <unordered_map>
#include "Graph.h"
#include "MSTFactory.cpp"
#include "TaskQueue.h"
#include "PipelineWorker.h"

using namespace std;

// Structure to hold per-client data
struct ClientSession {
    Graph* graph;
    mutex graph_mutex;  // Mutex to protect access to the client's graph
};

// Global variables
atomic<bool> running(true);  // Atomic flag to indicate whether the server is running
mutex coutMutex;             // Mutex to ensure thread-safe logging
mutex clients_mutex;         // Mutex to protect access to the clients list
mutex clientSessions_mutex;  // Mutex to protect access to clientSessions

// List to store all connected client sockets
vector<int> client_sockets;

// Map to store client sessions (per-client graphs and mutexes)
unordered_map<int, ClientSession*> clientSessions;

// Thread-safe logging function
void logMessage(const string& message) {
    lock_guard<mutex> lock(coutMutex);
    cout << message << endl;
}

// Task Queues for each stage
TaskQueue requestQueue;    // Stage 1: Parse and validate requests
TaskQueue processingQueue; // Stage 2: Perform MST calculations
TaskQueue responseQueue;   // Stage 3: Send response to the client

// Function to handle each client request
void handleClient(int client_sock, const string& request) {
    // Retrieve the client's session
    ClientSession* session = nullptr;
    {
        lock_guard<mutex> lock(clientSessions_mutex);
        auto it = clientSessions.find(client_sock);
        if (it != clientSessions.end()) {
            session = it->second;
        } else {
            // Client not found, perhaps disconnected
            logMessage("[Server] Client session not found for socket " + to_string(client_sock));
            return;
        }
    }

    string response;

    // Check for shutdown command
    if (request.find("shutdown") != string::npos) {
        response = "Server is shutting down.\n";
        // Enqueue the response to be sent before shutting down
        responseQueue.enqueue(Task(client_sock, response, [client_sock, response]() {
            ssize_t bytes_sent = send(client_sock, response.c_str(), response.size(), 0);
            if (bytes_sent == -1) {
                logMessage("[Server] Failed to send shutdown response to client " + to_string(client_sock) + ": " + strerror(errno));
            } else {
                logMessage("[Server] Shutdown response sent to client " + to_string(client_sock) + ".");
            }
            close(client_sock);
        }));
        running = false;

        // Stop all task queues to unblock worker threads
        requestQueue.stop();
        processingQueue.stop();
        responseQueue.stop();
        return;
    }

    // Process graph-related and MST-related requests
    if (request.find("Newgraph") != string::npos) {
        int vertices;
        if (sscanf(request.c_str(), "Newgraph %d", &vertices) == 1) {
            lock_guard<mutex> lock(session->graph_mutex);
            if (session->graph != nullptr) {
                delete session->graph;
            }
            session->graph = new Graph(vertices);
            response = "Graph initialized with " + to_string(vertices) + " vertices.\n";
        } else {
            response = "Invalid parameters for Newgraph.\n";
        }

    } else if (request.find("Newedge") != string::npos) {
        int u, v, weight;
        if (sscanf(request.c_str(), "Newedge %d %d %d", &u, &v, &weight) == 3) {
            lock_guard<mutex> lock(session->graph_mutex);
            if (session->graph) {
                // Validate vertex indices
                if (u < 0 || v < 0 || u >= session->graph->getVertices() || v >= session->graph->getVertices()) {
                    response = "Invalid vertex indices for Newedge command.\n";
                } else if (session->graph->addEdge(u, v, weight)) {
                    response = "Edge added: " + to_string(u) + " -> " + to_string(v) + " with weight " + to_string(weight) + ".\n";
                } else {
                    response = "Edge already exists.\n";
                }
            } else {
                response = "No graph initialized.\n";
            }
        } else {
            response = "Invalid edge parameters.\n";
        }

    } else if (request.find("Removeedge") != string::npos) {
        int u, v;
        if (sscanf(request.c_str(), "Removeedge %d %d", &u, &v) == 2) {
            lock_guard<mutex> lock(session->graph_mutex);
            if (session->graph) {
                // Validate vertex indices
                if (u < 0 || v < 0 || u >= session->graph->getVertices() || v >= session->graph->getVertices()) {
                    response = "Invalid vertex indices for Removeedge command.\n";
                } else if (session->graph->removeEdge(u, v)) {
                    response = "Removed edge: " + to_string(u) + " -> " + to_string(v) + ".\n";
                } else {
                    response = "Edge does not exist.\n";
                }
            } else {
                response = "No graph initialized.\n";
            }
        } else {
            response = "Invalid edge parameters.\n";
        }

    } else if (request.find("MST Prim") != string::npos || request.find("MST Kruskal") != string::npos) {
        lock_guard<mutex> lock(session->graph_mutex);
        if (session->graph == nullptr) {
            response = "No graph initialized.\n";
        } else {
            string algorithm = request.find("Prim") != string::npos ? "Prim" : "Kruskal";
            MSTSolver* solver = MSTFactory::getMSTSolver(algorithm);
            if (solver == nullptr) {
                response = "Invalid MST algorithm.\n";
            } else {
                try {
                    Tree mst = solver->solveMST(*(session->graph));
                    stringstream result;
                    result << "MST using " << algorithm << ":\n";
                    for (const auto& edge : mst.getEdges()) {
                        result << edge.second.first << " -- " << edge.second.second << " == " << edge.first << "\n";
                    }
                    result << "Total cost of MST: " << mst.getMSTCost() << "\n";
                    response = result.str();
                } catch (const std::exception& ex) {
                    response = "Error computing MST: ";
                    response += ex.what();
                    response += "\n";
                } catch (...) {
                    response = "Unknown error occurred while computing MST.\n";
                }
                delete solver;
            }
        }

    } else if (request.find("MST Longest") != string::npos) {
        lock_guard<mutex> lock(session->graph_mutex);
        if (session->graph == nullptr) {
            response = "No graph initialized.\n";
        } else {
            MSTSolver* solver = MSTFactory::getMSTSolver("Prim");
            if (solver == nullptr) {
                response = "Error initializing MST solver.\n";
            } else {
                try {
                    Tree mst = solver->solveMST(*(session->graph));
                    int longestDistance = mst.getLongestDistance();
                    response = "Longest distance in MST: " + to_string(longestDistance) + "\n";
                } catch (const std::exception& ex) {
                    response = "Error computing longest distance: ";
                    response += ex.what();
                    response += "\n";
                } catch (...) {
                    response = "Unknown error occurred while computing longest distance.\n";
                }
                delete solver;
            }
        }

    } else if (request.find("MST Shortest") != string::npos) {
        int src, dest;
        if (sscanf(request.c_str(), "MST Shortest %d %d", &src, &dest) == 2) {
            lock_guard<mutex> lock(session->graph_mutex);
            if (session->graph == nullptr) {
                response = "No graph initialized.\n";
            } else if (src < 0 || dest < 0 || src >= session->graph->getVertices() || dest >= session->graph->getVertices()) {
                response = "Invalid vertex indices for MST Shortest command.\n";
            } else {
                MSTSolver* solver = MSTFactory::getMSTSolver("Prim");
                if (solver == nullptr) {
                    response = "Error initializing MST solver.\n";
                } else {
                    try {
                        Tree mst = solver->solveMST(*(session->graph));
                        int shortestDistance = mst.getShortestDistance(src, dest);
                        response = "Shortest distance between " + to_string(src) + " and " + to_string(dest) + " in MST: " + to_string(shortestDistance) + "\n";
                    } catch (const std::exception& ex) {
                        response = "Error computing shortest distance: ";
                        response += ex.what();
                        response += "\n";
                    } catch (...) {
                        response = "Unknown error occurred while computing shortest distance.\n";
                    }
                    delete solver;
                }
            }
        } else {
            response = "Invalid command format. Use: MST Shortest <src> <dest>.\n";
        }

    } else if (request.find("MST Average") != string::npos) {
        lock_guard<mutex> lock(session->graph_mutex);
        if (session->graph == nullptr) {
            response = "No graph initialized.\n";
        } else {
            MSTSolver* solver = MSTFactory::getMSTSolver("Prim");
            if (solver == nullptr) {
                response = "Error initializing MST solver.\n";
            } else {
                try {
                    Tree mst = solver->solveMST(*(session->graph));
                    double avgDistance = mst.getAverageDistance();
                    response = "Average distance between vertices in MST: " + to_string(avgDistance) + "\n";
                } catch (const std::exception& ex) {
                    response = "Error computing average distance: ";
                    response += ex.what();
                    response += "\n";
                } catch (...) {
                    response = "Unknown error occurred while computing average distance.\n";
                }
                delete solver;
            }
        }

    } else {
        response = "Invalid command.\n";
    }

    // Enqueue the response to be sent
    responseQueue.enqueue(Task(client_sock, response, [client_sock, response]() {
        ssize_t bytes_sent = send(client_sock, response.c_str(), response.size(), 0);
        if (bytes_sent == -1) {
            logMessage("[Server] Failed to send response to client " + to_string(client_sock) + ": " + strerror(errno));
        } else {
            logMessage("[Server] Response sent to client " + to_string(client_sock) + ".");
        }
    }));
}

// Function to enqueue request parsing into the pipeline
void enqueueRequestParsing(int client_sock, const string& request) {
    logMessage("[Server] Enqueuing request parsing for client " + to_string(client_sock) + ".");
    requestQueue.enqueue(Task(client_sock, request, [client_sock, request]() {
        // Enqueue into processingQueue
        processingQueue.enqueue(Task(client_sock, request, [client_sock, request]() {
            handleClient(client_sock, request);
        }));
    }));
}

void startServer(int port) {
    int server_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
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
    if (listen(server_fd, 10) < 0) { // Increased backlog to handle more connections
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    logMessage("Pipeline server started on port " + to_string(port));

    // Start pipeline workers
    PipelineWorker requestWorker(requestQueue);       // Handles request parsing
    PipelineWorker processingWorker(processingQueue); // Handles processing
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

        // Lock the clients list before iterating
        clients_mutex.lock();
        for (const auto& client_sock : client_sockets) {
            FD_SET(client_sock, &readfds);
            if (client_sock > max_fd) {
                max_fd = client_sock;
            }
        }
        clients_mutex.unlock();

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

        // Check if there's a new incoming connection
        if (FD_ISSET(server_fd, &readfds)) {
            // Accept new connections in a loop to handle multiple simultaneous connection attempts
            while (true) {
                int new_client = accept(server_fd, (struct sockaddr*)&address, &addrlen);
                if (new_client < 0) {
                    if (errno == EWOULDBLOCK || errno == EAGAIN) {
                        // No more incoming connections
                        break;
                    } else {
                        perror("Accept failed");
                        running = false;
                        break;
                    }
                }

                // Set new_client to non-blocking
                int flags = fcntl(new_client, F_GETFL, 0);
                if (flags == -1) {
                    perror("fcntl F_GETFL new_client");
                    close(new_client);
                    continue;
                }
                if (fcntl(new_client, F_SETFL, flags | O_NONBLOCK) == -1) {
                    perror("fcntl F_SETFL new_client");
                    close(new_client);
                    continue;
                }

                // Add the new client to the clients list
                clients_mutex.lock();
                client_sockets.push_back(new_client);
                clients_mutex.unlock();

                // Initialize their session
                {
                    lock_guard<mutex> lock(clientSessions_mutex);
                    ClientSession* session = new ClientSession();
                    session->graph = nullptr;
                    clientSessions[new_client] = session;
                }

                logMessage("[Server] New client connected: " + to_string(new_client));
            }
        }

        // Iterate through all clients to check for incoming data
        clients_mutex.lock();
        for (auto it = client_sockets.begin(); it != client_sockets.end(); ) {
            int client_sock = *it;
            if (FD_ISSET(client_sock, &readfds)) {
                memset(buffer, 0, sizeof(buffer));
                int valread = read(client_sock, buffer, sizeof(buffer));
                if (valread > 0) {
                    std::string request(buffer, valread);
                    logMessage("[Server] Received request from client " + to_string(client_sock) + ": " + request);
                    enqueueRequestParsing(client_sock, request);
                } else if (valread == 0) {
                    // Client disconnected
                    logMessage("[Server] Client disconnected: " + to_string(client_sock));

                    // Remove their session
                    {
                        lock_guard<mutex> lock(clientSessions_mutex);
                        auto sessionIt = clientSessions.find(client_sock);
                        if (sessionIt != clientSessions.end()) {
                            ClientSession* session = sessionIt->second;
                            if (session->graph != nullptr) {
                                delete session->graph;
                            }
                            delete session;
                            clientSessions.erase(sessionIt);
                        }
                    }

                    close(client_sock);
                    it = client_sockets.erase(it);
                    continue; // Skip incrementing the iterator
                } else {
                    if (errno != EWOULDBLOCK && errno != EAGAIN) {
                        // Read error
                        logMessage("[Server] Read error on client " + to_string(client_sock) + ". Disconnecting.");

                        // Remove their session
                        {
                            lock_guard<mutex> lock(clientSessions_mutex);
                            auto sessionIt = clientSessions.find(client_sock);
                            if (sessionIt != clientSessions.end()) {
                                ClientSession* session = sessionIt->second;
                                if (session->graph != nullptr) {
                                    delete session->graph;
                                }
                                delete session;
                                clientSessions.erase(sessionIt);
                            }
                        }

                        close(client_sock);
                        it = client_sockets.erase(it);
                        continue; // Skip incrementing the iterator
                    }
                }
            }
            ++it;
        }
        clients_mutex.unlock();
    }

    // Close all client sockets
    clients_mutex.lock();
    for (const auto& client_sock : client_sockets) {
        close(client_sock);
    }
    clients_mutex.unlock();
    client_sockets.clear();

    // Clean up client sessions
    {
        lock_guard<mutex> lock(clientSessions_mutex);
        for (auto& entry : clientSessions) {
            ClientSession* session = entry.second;
            if (session->graph != nullptr) {
                delete session->graph;
            }
            delete session;
        }
        clientSessions.clear();
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
