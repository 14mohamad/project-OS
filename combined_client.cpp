#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>

using namespace std;

// Mutex for synchronized console output
mutex coutMutex;

// Function to send a command to the server and receive the response
void sendCommand(int client_id, int sock, const string& command) {
    char buffer[1024] = {0};

    // Send the command to the server
    ssize_t bytes_sent = send(sock, command.c_str(), command.size(), 0);
    if (bytes_sent == -1) {
        lock_guard<mutex> lock(coutMutex);
        cout << "[Client " << client_id << "] Error sending command: " << strerror(errno) << endl;
        return;
    }
    {
        lock_guard<mutex> lock(coutMutex);
        cout << "[Client " << client_id << "] Command sent: " << command;
    }

    // Read the response from the server
    ssize_t valread = read(sock, buffer, sizeof(buffer));
    if (valread > 0) {
        string response(buffer, valread);
        lock_guard<mutex> lock(coutMutex);
        cout << "[Client " << client_id << "] Server response:\n" << response << endl;
    } else if (valread == 0) {
        lock_guard<mutex> lock(coutMutex);
        cout << "[Client " << client_id << "] Server closed the connection." << endl;
    } else {
        lock_guard<mutex> lock(coutMutex);
        cout << "[Client " << client_id << "] No response from server or read error: " << strerror(errno) << endl;
    }
}

// Function representing a client
void clientFunction(int client_id, const vector<string>& commands) {
    int sock = 0;
    struct sockaddr_in serv_addr;

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        lock_guard<mutex> lock(coutMutex);
        cout << "[Client " << client_id << "] Socket creation error" << endl;
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080); // The port should match the server's port

    // Convert IPv4 address from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        lock_guard<mutex> lock(coutMutex);
        cout << "[Client " << client_id << "] Invalid address / Address not supported" << endl;
        close(sock);
        return;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        lock_guard<mutex> lock(coutMutex);
        cout << "[Client " << client_id << "] Connection failed" << endl;
        close(sock);
        return;
    }

    {
        lock_guard<mutex> lock(coutMutex);
        cout << "[Client " << client_id << "] Connected to the server." << endl;
    }

    // Loop through the commands and send them to the server
    for (const auto& command : commands) {
        sendCommand(client_id, sock, command);
        // Introduce a small delay between commands to simulate real-world usage
        this_thread::sleep_for(chrono::milliseconds(500));
    }

    // Close the socket after all commands have been sent
    close(sock);
    {
        lock_guard<mutex> lock(coutMutex);
        cout << "[Client " << client_id << "] Connection closed." << endl;
    }
}

int main() {
    // Define command lists for each client

    // Updated commands for Client 1
    vector<string> commands_client1 = {
        "Newgraph 5\n",           // Initialize a new graph with 5 vertices (indices 0 to 4)
        "Newedge 0 1 10\n",       // Add an edge from vertex 0 to vertex 1 with weight 10
        "Newedge 1 2 5\n",        // Add an edge from vertex 1 to vertex 2 with weight 5
        "Newedge 2 3 20\n",       // Add an edge from vertex 2 to vertex 3 with weight 20
        "Newedge 3 4 15\n",       // Add an edge from vertex 3 to vertex 4 with weight 15
        "MST Prim\n",             // Compute MST using Prim's algorithm
        "MST Kruskal\n",          // Compute MST using Kruskal's algorithm
        "MST Longest\n",          // Compute the longest distance in the MST
        "MST Shortest 0 4\n",     // Compute the shortest distance between vertex 0 and vertex 4
        "MST Average\n"           // Compute the average distance between all vertex pairs in the MST
    };

    // Updated commands for Client 2
    vector<string> commands_client2 = {
        "Newgraph 6\n",           // Initialize a new graph with 6 vertices (indices 0 to 5)
        "Newedge 0 1 7\n",        // Add edges to form a connected graph
        "Newedge 1 2 8\n",
        "Newedge 2 3 9\n",
        "Newedge 3 4 10\n",
        "Newedge 4 5 5\n",
        "Removeedge 1 2\n",       // Remove the edge from vertex 1 to vertex 2
        "MST Prim\n",             // Compute MST using Prim's algorithm after the edge is removed
    };

    // Create threads for each client
    thread client1(clientFunction, 1, commands_client1);
    thread client2(clientFunction, 2, commands_client2);

    // Wait for both clients to finish
    client1.join();
    client2.join();

    return 0;
}
