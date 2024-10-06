#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <vector>

using namespace std;

// Function to send a command to the server and receive the response
void sendCommand(int sock, const string& command) {
    char buffer[1024] = {0};

    // Send the command to the server
    send(sock, command.c_str(), command.size(), 0);
    cout << "[Client] Command sent: " << command << endl;

    // Read the response from the server
    int valread = read(sock, buffer, 1024);
    if (valread > 0) {
        cout << "[Client] Server response: " << string(buffer, valread) << endl;
    } else {
        cout << "[Client] No response from server." << endl;
    }
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cout << "[Client] Socket creation error" << endl;
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080); // The port should match the server's port

    // Convert IPv4 address from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        cout << "[Client] Invalid address / Address not supported" << endl;
        return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        cout << "[Client] Connection failed" << endl;
        return -1;
    }

    cout << "[Client] Connected to the server." << endl;

    // Define a list of commands to send to the server, covering both types of servers
    vector<string> commands = {
        "Newgraph 5\n",           // Initialize a new graph with 5 vertices
        "Newedge 0 1 10\n",       // Add an edge from vertex 0 to vertex 1 with weight 10
        "Newedge 1 2 5\n",        // Add an edge from vertex 1 to vertex 2 with weight 5
        "Newedge 2 3 20\n",       // Add an edge from vertex 2 to vertex 3 with weight 20
        "Newedge 3 4 25\n",       // Add an edge from vertex 3 to vertex 4 with weight 25
        "MST Prim\n",             // Compute MST using Prim's algorithm
        "MST Kruskal\n",          // Compute MST using Kruskal's algorithm
        "MST Longest\n",          // Compute the longest distance in the MST
        "MST Shortest 0 4\n",     // Compute the shortest distance between vertex 0 and vertex 4
        "MST Average\n",          // Compute the average distance between all vertex pairs in the MST
        "Removeedge 1 2\n",       // Remove the edge from vertex 1 to vertex 2
        "MST Prim\n",             // Recompute MST using Prim's algorithm after the edge is removed
        "shutdown\n"              // Send shutdown command to the server
    };

    // Loop through the commands and send them to the server
    for (const auto& command : commands) {
        sendCommand(sock, command);
        // Optionally, you can introduce a delay between commands with sleep(1);
    }

    // Test repeated requests to trigger shutdown due to repeated requests (for Leader-Follower server)
    cout << "\nTriggering shutdown via repeated requests:\n";
    for (int i = 0; i < 6; ++i) {
        sendCommand(sock, "Newedge 0 1 1\n");  // Send the same command 6 times to trigger repeated request shutdown
    }

    // Close the socket after all commands have been sent
    close(sock);
    cout << "[Client] Connection closed." << endl;

    return 0;
}
