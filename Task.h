#pragma once
#include <functional>
#include <string>

class Task {
public:
    int client_sock;  // Client socket for communication
    std::string command;   // Client command (e.g., Newgraph, Newedge, MST Prim)
    std::function<void()> operation;  // The actual operation to be performed (function pointer or lambda)

    // Default constructor
    Task() : client_sock(-1), command(""), operation(nullptr) {}

    // Parameterized constructor
    Task(int sock, const std::string& cmd, std::function<void()> op) 
        : client_sock(sock), command(cmd), operation(op) {}
};
