/*
 * server.cpp - TCP Chat Server (Windows / Winsock2)
 *
 * This program listens on port 8080 for a single client connection.
 * Once connected, it supports bidirectional text messaging using two threads:
 *   - Main thread: reads keyboard input and sends messages to the client
 *   - Background thread: receives messages from the client and prints them
 */

#include <iostream>
#include <thread>
#include <winsock2.h>   // Windows Sockets API (Winsock) - core networking functions
#include <ws2tcpip.h>   // Extended socket helpers (e.g. inet_pton)
using namespace std;

// Automatically link the Winsock library at compile time (MSVC / MinGW)
#pragma comment(lib, "ws2_32.lib")

#define PORT 8080           // TCP port the server listens on (must match client)
#define BUFFER_SIZE 1024    // Max bytes received per recv() call

// Global socket handle for the connected client (used by the receiver thread)
SOCKET clientSocket;

/*
 * receiveMessages - Runs in a separate thread.
 * Continuously blocks on recv() waiting for data from the client.
 * When data arrives, it is null-terminated and printed to the console.
 */
void receiveMessages()
{
    char buffer[BUFFER_SIZE];

    while (true)
    {
        // Block until the client sends data (or the connection closes)
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);

        // recv() returns 0 when the peer closes the connection, or SOCKET_ERROR on failure
        if (bytesReceived <= 0)
        {
            cout << "\nClient disconnected.\n";
            break;
        }

        // Treat received bytes as a C-string (leave room for null terminator)
        buffer[bytesReceived] = '\0';

        cout << "\nClient: " << buffer << endl;
        cout << "Server: ";
        cout.flush();   // Re-print prompt immediately after incoming message
    }
}

int main()
{
    WSADATA wsData;

    // Step 1: Initialize Winsock (required before any socket call on Windows)
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0)
    {
        cerr << "WSAStartup failed\n";
        return 1;
    }

    // Step 2: Create a TCP stream socket (AF_INET = IPv4, SOCK_STREAM = TCP)
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (serverSocket == INVALID_SOCKET)
    {
        cerr << "Socket creation failed\n";
        return 1;
    }

    // Step 3: Configure the local address: listen on all network interfaces, port 8080
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);              // Convert port to network byte order
    serverAddr.sin_addr.s_addr = INADDR_ANY;        // 0.0.0.0 - accept connections on any local IP

    // Step 4: Bind the socket to the address/port so the OS knows where to deliver incoming packets
    if (bind(serverSocket,
             (sockaddr*)&serverAddr,
             sizeof(serverAddr)) == SOCKET_ERROR)
    {
        cerr << "Bind failed\n";
        return 1;
    }

    // Step 5: Put socket in listening mode; backlog of 1 pending connection
    listen(serverSocket, 1);

    cout << "Waiting for client to connect..." << endl;

    // Step 6: Block until a client completes the TCP three-way handshake
    clientSocket = accept(serverSocket, nullptr, nullptr);

    if (clientSocket == INVALID_SOCKET)
    {
        cerr << "Accept failed\n";
        return 1;
    }

    cout << "Client connected!" << endl;

    // Step 7: Start background thread to receive messages while main thread sends
    thread receiver(receiveMessages);

    string message;

    // Step 8: Main loop - read user input and send to client over TCP
    while (true)
    {
        cout << "Server: ";
        getline(cin, message);

        send(
            clientSocket,
            message.c_str(),
            static_cast<int>(message.length()),
            0
        );

        // Typing "exit" ends the chat session on this side
        if (message == "exit")
            break;
    }

    // Step 9: Clean up sockets and Winsock resources
    closesocket(clientSocket);
    closesocket(serverSocket);

    WSACleanup();

    receiver.join();   // Wait for receiver thread to finish before exiting

    return 0;
}
