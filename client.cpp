/*
 * client.cpp - TCP Chat Client (Windows / Winsock2)
 *
 * Connects to a server at a fixed IPv4 address on port 8080.
 * Supports bidirectional text messaging using two threads:
 *   - Main thread: reads keyboard input and sends messages to the server
 *   - Background thread: receives messages from the server and prints them
 */

#include <iostream>
#include <thread>
#include <winsock2.h>   // Windows Sockets API (Winsock) - core networking functions
#include <ws2tcpip.h>   // Extended socket helpers (e.g. inet_pton)
using namespace std;

// Automatically link the Winsock library at compile time (MSVC / MinGW)
#pragma comment(lib, "ws2_32.lib")

#define PORT 8080           // TCP port on the server (must match server.cpp)
#define BUFFER_SIZE 1024    // Max bytes received per recv() call

// Global socket handle for the server connection (used by the receiver thread)
SOCKET serverSocket;

/*
 * receiveMessages - Runs in a separate thread.
 * Continuously blocks on recv() waiting for data from the server.
 * When data arrives, it is null-terminated and printed to the console.
 */
void receiveMessages()
{
    char buffer[BUFFER_SIZE];

    while (true)
    {
        // Block until the server sends data (or the connection closes)
        int bytesReceived = recv(serverSocket, buffer, BUFFER_SIZE - 1, 0);

        // recv() returns 0 when the peer closes the connection, or SOCKET_ERROR on failure
        if (bytesReceived <= 0)
        {
            cout << "\nDisconnected from server.\n";
            break;
        }

        // Treat received bytes as a C-string (leave room for null terminator)
        buffer[bytesReceived] = '\0';

        cout << "\nServer: " << buffer << endl;
        cout << "Client: ";
        cout.flush();   // Re-print prompt immediately after incoming message
    }
}

int main()
{
    // IPv4 address of the machine running server.exe (update to match your server PC)
    const char* SERVER_IP = "192.168.0.109";

    WSADATA wsData;

    // Step 1: Initialize Winsock (required before any socket call on Windows)
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0)
    {
        cerr << "WSAStartup failed\n";
        return 1;
    }

    // Step 2: Create a TCP stream socket (AF_INET = IPv4, SOCK_STREAM = TCP)
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (serverSocket == INVALID_SOCKET)
    {
        cerr << "Socket creation failed\n";
        return 1;
    }

    // Step 3: Configure the remote server address (IP + port)
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);              // Convert port to network byte order

    // Convert human-readable IP string (e.g. "192.168.0.107") to binary form
    inet_pton(
        AF_INET,
        SERVER_IP,
        &serverAddr.sin_addr
    );

    // Step 4: Initiate TCP connection (three-way handshake with the server)
    if (connect(
            serverSocket,
            (sockaddr*)&serverAddr,
            sizeof(serverAddr)
        ) == SOCKET_ERROR)
    {
        cerr << "Connection failed\n";
        return 1;
    }

    cout << "Connected to server!" << endl;

    // Step 5: Start background thread to receive messages while main thread sends
    thread receiver(receiveMessages);

    string message;

    // Step 6: Main loop - read user input and send to server over TCP
    while (true)
    {
        cout << "Client: ";
        getline(cin, message);

        send(
            serverSocket,
            message.c_str(),
            static_cast<int>(message.length()),
            0
        );

        // Typing "exit" ends the chat session on this side
        if (message == "exit")
            break;
    }

    // Step 7: Clean up socket and Winsock resources
    closesocket(serverSocket);

    WSACleanup();

    receiver.join();   // Wait for receiver thread to finish before exiting

    return 0;
}
