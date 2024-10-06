#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "Graph.h"
#include "MSTFactory.cpp"  // Include your MST logic
#include <cstring>
#include <sstream>
#include <unordered_map>
#include <atomic>

using namespace std;

// Shared resources
Graph* currentGraph = nullptr;  // To store the graph's state
unordered_map<string, int> requestCount;  // To track the count of each request

// Mutexes to protect shared resources
mutex graphMutex;
mutex requestCountMutex;

// Mutex for synchronizing std::cout
mutex coutMutex;

// Atomic flag for shutdown
atomic<bool> isShuttingDown(false);

// Thread-safe logging function (optional)
void logMessage(const string& message) {
    lock_guard<mutex> lock(coutMutex);
    cout << message << endl;
}

// Function to handle each client request
void handleClient(int client_sock) {
    char buffer[1024] = {0};

    while (true) {
        memset(buffer, 0, sizeof(buffer));  // Clear buffer for each request
        int valread = read(client_sock, buffer, 1024);

        if (valread > 0) {
            string request(buffer);

            // Log received request
            logMessage("[Server] Received request: " + request);

            string response;

            // Check for shutdown command
            if (request.find("shutdown") != string::npos) {
                response = "Server is shutting down.";
                send(client_sock, response.c_str(), response.size(), 0);
                close(client_sock);
                isShuttingDown = true;
                return;
            }

            // Process graph-related and MST-related requests
            if (request.find("Newgraph") != string::npos) {
                int vertices;
                sscanf(buffer, "Newgraph %d", &vertices);

                {
                    lock_guard<mutex> lock(graphMutex);
                    if (currentGraph != nullptr) {
                        delete currentGraph;
                    }
                    currentGraph = new Graph(vertices);
                }

                response = "Graph initialized with " + to_string(vertices) + " vertices.";

            } else if (request.find("Newedge") != string::npos) {
                int u, v, weight;
                if (sscanf(buffer, "Newedge %d %d %d", &u, &v, &weight) == 3) {
                    lock_guard<mutex> lock(graphMutex);
                    if (currentGraph) {
                        if (currentGraph->addEdge(u, v, weight)) {
                            response = "Edge added: " + to_string(u) + " -> " + to_string(v) + " with weight " + to_string(weight);
                        } else {
                            response = "Edge already exists.";
                        }
                    } else {
                        response = "No graph initialized.";
                    }
                } else {
                    response = "Invalid edge parameters.";
                }

            } else if (request.find("Removeedge") != string::npos) {
                int u, v;
                if (sscanf(buffer, "Removeedge %d %d", &u, &v) == 2) {
                    lock_guard<mutex> lock(graphMutex);
                    if (currentGraph) {
                        if (currentGraph->removeEdge(u, v)) {
                            response = "Removed edge: " + to_string(u) + " -> " + to_string(v);
                        } else {
                            response = "Edge does not exist.";
                        }
                    } else {
                        response = "No graph initialized.";
                    }
                } else {
                    response = "Invalid edge parameters.";
                }

            } else if (request.find("MST Prim") != string::npos || request.find("MST Kruskal") != string::npos) {
                lock_guard<mutex> lock(graphMutex);
                if (currentGraph == nullptr) {
                    response = "No graph initialized.";
                } else {
                    string algorithm = request.find("Prim") != string::npos ? "Prim" : "Kruskal";
                    MSTSolver* solver = MSTFactory::getMSTSolver(algorithm);
                    Tree mst = solver->solveMST(*currentGraph);
                    stringstream result;
                    result << "MST using " << algorithm << ":\n";
                    for (const auto& edge : mst.getEdges()) {
                        result << edge.second.first << " -- " << edge.second.second << " == " << edge.first << "\n";
                    }
                    result << "Total cost of MST: " << mst.getMSTCost() << "\n";
                    response = result.str();
                    delete solver;
                }

            // New commands for additional MST features
            } else if (request.find("MST Longest") != string::npos) {
                lock_guard<mutex> lock(graphMutex);
                if (currentGraph == nullptr) {
                    response = "No graph initialized.";
                } else {
                    MSTSolver* solver = MSTFactory::getMSTSolver("Prim");  // Choose any MST algorithm
                    Tree mst = solver->solveMST(*currentGraph);
                    int longestDistance = mst.getLongestDistance();
                    response = "Longest distance in MST: " + to_string(longestDistance) + "\n";
                    delete solver;
                }

            } else if (request.find("MST Shortest") != string::npos) {
                int src, dest;
                if (sscanf(buffer, "MST Shortest %d %d", &src, &dest) == 2) {
                    lock_guard<mutex> lock(graphMutex);
                    if (currentGraph == nullptr) {
                        response = "No graph initialized.";
                    } else {
                        MSTSolver* solver = MSTFactory::getMSTSolver("Prim");  // Choose any MST algorithm
                        Tree mst = solver->solveMST(*currentGraph);
                        int shortestDistance = mst.getShortestDistance(src, dest);
                        response = "Shortest distance between " + to_string(src) + " and " + to_string(dest) + " in MST: " + to_string(shortestDistance) + "\n";
                        delete solver;
                    }
                } else {
                    response = "Invalid command format. Use: MST Shortest <src> <dest>.\n";
                }

            } else if (request.find("MST Average") != string::npos) {
                lock_guard<mutex> lock(graphMutex);
                if (currentGraph == nullptr) {
                    response = "No graph initialized.";
                } else {
                    MSTSolver* solver = MSTFactory::getMSTSolver("Prim");  // Choose any MST algorithm
                    Tree mst = solver->solveMST(*currentGraph);
                    double avgDistance = mst.getAverageDistance();
                    response = "Average distance between vertices in MST: " + to_string(avgDistance) + "\n";
                    delete solver;
                }

            } else {
                response = "Invalid command.\n";
            }

            // Send response to the client
            send(client_sock, response.c_str(), response.size(), 0);
            logMessage("[Server] Response sent for request: " + request);
        } else {
            logMessage("[Server] Client disconnected or no more data to read.");
            break;
        }
    }

    close(client_sock);  // Close the connection after handling all requests
}

// Class to manage the leader-follower thread pool
class LeaderFollowerPool {
private:
    vector<thread> threads;
    mutex mtx;
    condition_variable cv;
    bool stop = false;
    int leaderIdx = -1;
    int server_fd;

public:
    LeaderFollowerPool(size_t numThreads, int server_fd) : server_fd(server_fd) {
        for (size_t i = 0; i < numThreads; ++i) {
            threads.emplace_back([this, i] { worker(i); });
        }
    }

    ~LeaderFollowerPool() {
        {
            unique_lock<mutex> lock(mtx);
            stop = true;
        }
        cv.notify_all();

        for (thread &t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    void worker(int threadId) {
        while (true) {
            int client_sock = -1;

            // Leader election and connection handling
            {
                unique_lock<mutex> lock(mtx);

                // Wait until this thread becomes the leader
                cv.wait(lock, [this, threadId] { return stop || leaderIdx == -1 || leaderIdx == threadId; });
                if (stop || isShuttingDown) return;

                // This thread becomes the leader
                leaderIdx = threadId;

                // Log leadership
                logMessage("[Leader-Follower] Thread " + to_string(threadId) + " is the leader.");

                // Accept a new client connection
                struct sockaddr_in clientAddr;
                socklen_t clientAddrLen = sizeof(clientAddr);
                client_sock = accept(server_fd, (struct sockaddr*)&clientAddr, &clientAddrLen);
                if (client_sock < 0) {
                    perror("Accept failed");
                    // Release leadership and notify others to try again
                    leaderIdx = -1;
                    cv.notify_all();
                    continue;
                }

                // After handling the connection, release the leader position
                leaderIdx = -1;
                cv.notify_all();  // Notify other followers to compete for the next leadership
            }

            // Handle the client outside of the critical section
            handleClient(client_sock);

            // If shutdown has been triggered, stop the thread
            if (isShuttingDown) return;
        }
    }
};

// Function to start the Leader-Follower Thread Pool server
void startServer(int port, size_t numThreads) {
    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Create the server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Allow the socket to be reusable immediately after the program terminates
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind the socket to the address and port
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Start listening for connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    logMessage("Leader-Follower server started on port " + to_string(port));

    // Create a Leader-Follower pool with the given number of threads
    LeaderFollowerPool pool(numThreads, server_fd);

    // Keep the main thread alive while the pool is running
    while (!isShuttingDown) {
        this_thread::sleep_for(chrono::seconds(1));
    }

    // Shut down the server socket to stop accepting new connections
    shutdown(server_fd, SHUT_RDWR);

    // Clean up before shutting down
    {
        lock_guard<mutex> lock(graphMutex);
        if (currentGraph != nullptr) {
            delete currentGraph;
            currentGraph = nullptr;
        }
    }

    close(server_fd);
    logMessage("Server has shut down.");
}

int main() {
    // Start the server with a Leader-Follower pool of 4 threads
    startServer(8080, 4);

    return 0;
}
