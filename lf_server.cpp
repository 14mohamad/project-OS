// leader_follower_server.cpp

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
#include <errno.h>

using namespace std;

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
    string partialCommandBuffer;
    Graph* clientGraph = nullptr;  // Each client has its own graph

    while (true) {
        memset(buffer, 0, sizeof(buffer));  // Clear buffer for each read
        int valread = read(client_sock, buffer, 1024);

        if (valread > 0) {
            partialCommandBuffer.append(buffer, valread);

            size_t pos;
            while ((pos = partialCommandBuffer.find('\n')) != string::npos) {
                string request = partialCommandBuffer.substr(0, pos);
                partialCommandBuffer.erase(0, pos + 1);  // Remove processed command

                // Trim any carriage return
                if (!request.empty() && request.back() == '\r') {
                    request.pop_back();
                }

                // Log received request
                logMessage("[Server] Received request: " + request);

                string response;

                // Check for shutdown command
                if (request.find("shutdown") != string::npos) {
                    response = "Server is shutting down.\n";
                    send(client_sock, response.c_str(), response.size(), 0);
                    close(client_sock);
                    isShuttingDown = true;
                    return;
                }

                // Process graph-related and MST-related requests
                if (request.find("Newgraph") != string::npos) {
                    int vertices;
                    if (sscanf(request.c_str(), "Newgraph %d", &vertices) == 1) {
                        if (clientGraph != nullptr) {
                            delete clientGraph;
                        }
                        clientGraph = new Graph(vertices);

                        response = "Graph initialized with " + to_string(vertices) + " vertices.\n";
                    } else {
                        response = "Invalid Newgraph command format.\n";
                    }

                } else if (request.find("Newedge") != string::npos) {
                    int u, v, weight;
                    if (sscanf(request.c_str(), "Newedge %d %d %d", &u, &v, &weight) == 3) {
                        if (clientGraph) {
                            if (clientGraph->addEdge(u, v, weight)) {
                                response = "Edge added: " + to_string(u) + " -> " + to_string(v) + " with weight " + to_string(weight) + "\n";
                            } else {
                                response = "Edge already exists.\n";
                            }
                        } else {
                            response = "No graph initialized.\n";
                        }
                    } else {
                        response = "Invalid Newedge command format.\n";
                    }

                } else if (request.find("Removeedge") != string::npos) {
                    int u, v;
                    if (sscanf(request.c_str(), "Removeedge %d %d", &u, &v) == 2) {
                        if (clientGraph) {
                            if (clientGraph->removeEdge(u, v)) {
                                response = "Removed edge: " + to_string(u) + " -> " + to_string(v) + "\n";
                            } else {
                                response = "Edge does not exist.\n";
                            }
                        } else {
                            response = "No graph initialized.\n";
                        }
                    } else {
                        response = "Invalid Removeedge command format.\n";
                    }

                } else if (request.find("MST Prim") != string::npos || request.find("MST Kruskal") != string::npos) {
                    if (clientGraph == nullptr) {
                        response = "No graph initialized.\n";
                    } else {
                        string algorithm = request.find("Prim") != string::npos ? "Prim" : "Kruskal";
                        MSTSolver* solver = MSTFactory::getMSTSolver(algorithm);
                        Tree mst = solver->solveMST(*clientGraph);
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
                    if (clientGraph == nullptr) {
                        response = "No graph initialized.\n";
                    } else {
                        MSTSolver* solver = MSTFactory::getMSTSolver("Prim");  // Choose any MST algorithm
                        Tree mst = solver->solveMST(*clientGraph);
                        int longestDistance = mst.getLongestDistance();
                        response = "Longest distance in MST: " + to_string(longestDistance) + "\n";
                        delete solver;
                    }

                } else if (request.find("MST Shortest") != string::npos) {
                    int src, dest;
                    if (sscanf(request.c_str(), "MST Shortest %d %d", &src, &dest) == 2) {
                        if (clientGraph == nullptr) {
                            response = "No graph initialized.\n";
                        } else {
                            MSTSolver* solver = MSTFactory::getMSTSolver("Prim");  // Choose any MST algorithm
                            Tree mst = solver->solveMST(*clientGraph);
                            int shortestDistance = mst.getShortestDistance(src, dest);
                            response = "Shortest distance between " + to_string(src) + " and " + to_string(dest) + " in MST: " + to_string(shortestDistance) + "\n";
                            delete solver;
                        }
                    } else {
                        response = "Invalid MST Shortest command format.\n";
                    }

                } else if (request.find("MST Average") != string::npos) {
                    if (clientGraph == nullptr) {
                        response = "No graph initialized.\n";
                    } else {
                        MSTSolver* solver = MSTFactory::getMSTSolver("Prim");  // Choose any MST algorithm
                        Tree mst = solver->solveMST(*clientGraph);
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
            }
        } else if (valread == 0) {
            logMessage("[Server] Client disconnected.");
            break;
        } else {
            logMessage("[Server] Read error: " + string(strerror(errno)));
            break;
        }
    }

    // Clean up client's graph
    if (clientGraph != nullptr) {
        delete clientGraph;
        clientGraph = nullptr;
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

    close(server_fd);
    logMessage("Server has shut down.");
}

int main() {
    // Start the server with a Leader-Follower pool of 4 threads
    startServer(8080, 4);

    return 0;
}
