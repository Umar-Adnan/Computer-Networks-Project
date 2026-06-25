# Computer Networks Project Report
## TCP Client–Server Chat Application (Windows / Winsock2)

---

## 1. Introduction

This project implements a simple **text-based chat application** using the **client–server model** over **TCP/IP**. Two C++ programs communicate across a local network:

| Program       | Role   | File          |
|---------------|--------|---------------|
| `server.exe`  | Server | `server.cpp`  |
| `client.exe`  | Client | `client.cpp`  |

The server waits for an incoming connection. The client connects to the server’s IP address. Once connected, both sides can type messages and see messages from the other side in real time.

Both programs were compiled using **MSYS2** (MinGW toolchain) and executed from **Windows Command Prompt (CMD)**. The server machine’s IPv4 address was **192.168.0.107**, and **Windows Firewall had to be disabled** on the server PC for the client to connect successfully.

---

## 2. Network Model and Protocol

### 2.1 Client–Server Architecture

```
┌─────────────────────┐                    ┌─────────────────────┐
│   CLIENT MACHINE    │                    │   SERVER MACHINE    │
│                     │                    │                     │
│  client.exe         │    TCP over IP     │  server.exe         │
│  (actively connects)│ ─────────────────► │  (passive listens)  │
│                     │   Port 8080        │                     │
│  IP: any on LAN     │                    │  IP: 192.168.0.107  │
└─────────────────────┘                    └─────────────────────┘
```

- **Server (passive open):** Creates a socket, binds to port **8080** on all local interfaces (`INADDR_ANY`), calls `listen()`, then blocks on `accept()` until a client connects.
- **Client (active open):** Creates a socket and calls `connect()` to the server’s IP (**192.168.0.107**) and port **8080**.

### 2.2 Transport Layer: TCP

Both programs use:

- **Protocol:** TCP (`SOCK_STREAM`, `IPPROTO_TCP`)
- **Address family:** IPv4 (`AF_INET`)
- **Port:** `8080` (defined as `#define PORT 8080` in both files)

TCP provides:

- **Reliable delivery** — data arrives in order without loss (under normal conditions)
- **Connection-oriented communication** — a session is established via the **three-way handshake** (`SYN → SYN-ACK → ACK`) before any data is sent
- **Full-duplex** — both sides can send and receive simultaneously

### 2.3 Application Layer

The application sends plain text strings. There is no custom message format, length prefix, or encryption. Each `send()` transmits exactly the bytes of the typed line (without a trailing newline unless the user typed one).

---

## 3. Technologies and Libraries

| Component        | Purpose                                              |
|------------------|------------------------------------------------------|
| **C++**          | Implementation language                              |
| **Winsock2**     | Windows API for socket programming (`winsock2.h`)    |
| **ws2tcpip.h**   | Modern IP address helpers (`inet_pton`)              |
| **ws2_32.lib**   | Winsock library linked via `#pragma comment`         |
| **`<thread>`**   | C++11 threads for concurrent send/receive            |
| **MSYS2 / g++**  | Cross-compilation environment used to build `.exe`   |
| **CMD**          | Used to run the compiled executables on Windows      |

---

## 4. How the Server Program Works (`server.cpp`)

### 4.1 High-Level Flow

1. Initialize Winsock (`WSAStartup`)
2. Create a TCP socket
3. Bind to `0.0.0.0:8080` (all network interfaces)
4. Listen for incoming connections
5. Accept one client
6. Start a receiver thread
7. Main thread sends messages typed by the user
8. Clean up on `"exit"` or disconnect

### 4.2 Step-by-Step Explanation

#### Step 1 — `WSAStartup(MAKEWORD(2, 2), &wsData)`

On Windows, every program using sockets must initialize the Winsock DLL before calling any socket function and call `WSACleanup()` when done. `MAKEWORD(2, 2)` requests Winsock version 2.2.

#### Step 2 — `socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)`

Creates an endpoint for communication:

- `AF_INET` → IPv4
- `SOCK_STREAM` → byte stream (TCP)
- Returns a **listening socket** descriptor

#### Step 3 — `bind()` and address setup

```cpp
serverAddr.sin_family = AF_INET;
serverAddr.sin_port = htons(PORT);
serverAddr.sin_addr.s_addr = INADDR_ANY;
```

- `htons(PORT)` converts port 8080 from **host byte order** to **network byte order** (big-endian), required for cross-platform compatibility.
- `INADDR_ANY` (`0.0.0.0`) means the server accepts connections on **any local network interface** (Wi‑Fi, Ethernet, loopback, etc.).

`bind()` associates the socket with that IP/port. If another program already uses port 8080, `bind()` fails.

#### Step 4 — `listen(serverSocket, 1)`

Puts the socket into **passive mode**. The OS maintains a queue of pending connections; backlog `1` allows one connection waiting while another is being accepted.

#### Step 5 — `accept()`

Blocks until a client completes the TCP handshake. Returns a **new socket** (`clientSocket`) dedicated to that client. The original `serverSocket` continues listening (though this program only accepts one client total).

#### Step 6 — Multi-threading

| Thread        | Responsibility                                      |
|---------------|-----------------------------------------------------|
| **Main**      | Reads `"Server: "` input, calls `send()` to client  |
| **Receiver**  | Calls `recv()` in a loop, prints `"Client: "` msgs |

Without threading, the program could either send **or** receive at one time — not both. Threading enables a chat-like experience where incoming messages appear while you are typing.

The `receiveMessages()` function:

- Allocates a 1024-byte buffer
- Calls `recv()` (blocking)
- Null-terminates received data: `buffer[bytesReceived] = '\0'`
- Prints the message and re-displays the `"Server: "` prompt

If `recv()` returns `0` or negative, the client has disconnected.

#### Step 7 — Sending and exiting

The main loop reads a line with `getline(cin, message)` and sends it with `send()`. Typing **`exit`** breaks the loop, closes sockets, calls `WSACleanup()`, and `join()`s the receiver thread.

---

## 5. How the Client Program Works (`client.cpp`)

### 5.1 High-Level Flow

1. Initialize Winsock
2. Create a TCP socket
3. Set server address to `192.168.0.107:8080`
4. Connect to the server
5. Start a receiver thread
6. Main thread sends messages typed by the user
7. Clean up on `"exit"` or disconnect

### 5.2 Step-by-Step Explanation

#### Server IP address

```cpp
const char* SERVER_IP = "192.168.0.107";
```

This is the **IPv4 address of the machine running `server.exe`** on the local network (LAN). It was obtained from `ipconfig` on the server PC (also recorded in `New Text Document.txt`).

`inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr)` converts the dotted-decimal string into the binary `sin_addr` field required by `connect()`.

#### `connect()`

Initiates the TCP three-way handshake to the server. On success, a bidirectional byte stream is established. On failure, the program prints `"Connection failed"` and exits.

Possible failure reasons (relevant to this project):

- Server not running
- Wrong IP address
- Firewall blocking port 8080 on the server
- Client and server on different networks with no route

#### Multi-threading (same pattern as server)

| Thread        | Responsibility                                       |
|---------------|------------------------------------------------------|
| **Main**      | Reads `"Client: "` input, calls `send()` to server    |
| **Receiver**  | Calls `recv()` in a loop, prints `"Server: "` msgs   |

---

## 6. Complete Communication Sequence

```
SERVER                              CLIENT
  |                                   |
  |  (server running, listen:8080)  |
  |                                   |
  |         SYN                       |
  | <──────────────────────────────── |
  |         SYN-ACK                   |
  | ────────────────────────────────> |
  |         ACK                       |
  | <──────────────────────────────── |
  |                                   |
  |  "Client connected!"              |  "Connected to server!"
  |                                   |
  |  recv thread started              |  recv thread started
  |                                   |
  | <──────── "Hello" ────────────────|  user types Hello, send()
  |  receiveMessages prints it        |
  |                                   |
  |  user types Hi, send()            |
  | ──────────── "Hi" ──────────────> |
  |                                   |  receiveMessages prints it
  |                                   |
  | <──────── "exit" ─────────────────|  (either side can exit)
  |  closes sockets                   |  closes socket
```

---

## 7. Compilation with MSYS2

MSYS2 provides a Unix-like shell on Windows with **MinGW-w64** (`g++`). Typical compile commands:

```bash
g++ server.cpp -o server.exe -lws2_32 -std=c++11
g++ client.cpp -o client.exe -lws2_32 -std=c++11
```

| Flag           | Purpose                                                |
|----------------|--------------------------------------------------------|
| `-o server.exe`| Output executable name                                 |
| `-lws2_32`     | Link against Winsock library (also done via `#pragma`) |
| `-std=c++11`   | Required for `std::thread`                             |

The `#pragma comment(lib, "ws2_32.lib")` in both source files tells the linker to include Winsock automatically on MSVC; with MinGW, `-lws2_32` achieves the same result.

After compilation, the `.exe` files can be run from **CMD** or MSYS2 terminal.

---

## 8. Execution with CMD

### 8.1 On the Server Machine (192.168.0.107)

1. Open **CMD**
2. Navigate to the folder containing `server.exe`
3. Run:

```cmd
server.exe
```

Expected output:

```
Waiting for client to connect...
```

After the client connects:

```
Client connected!
Server:
```

### 8.2 On the Client Machine

1. Open **CMD**
2. Navigate to the folder containing `client.exe`
3. Ensure `SERVER_IP` in `client.cpp` matches the server’s LAN IP
4. Run:

```cmd
client.exe
```

Expected output:

```
Connected to server!
Client:
```

### 8.3 Chatting

- Type a message and press **Enter** to send
- Incoming messages appear on a new line with a `"Server: "` or `"Client: "` prefix
- Type **`exit`** on either side to end the session

---

## 9. Windows Firewall and Why It Was Needed

When the server calls `bind()` and `listen()` on port **8080**, it opens a **listening port** on the network interface. By default, **Windows Defender Firewall** blocks unsolicited incoming TCP connections to protect the machine.

Symptoms when the firewall blocks the connection:

- Server shows `Waiting for client to connect...` but never `Client connected!`
- Client shows `Connection failed`

**Disabling the firewall** (as required in this project) removes that block and allows the client’s TCP SYN packet to reach `server.exe`.

### Safer alternatives (recommended for real use)

Instead of disabling the entire firewall:

1. Add an **inbound rule** allowing TCP port **8080** for `server.exe`
2. Or allow the specific application through the firewall in **Windows Security → Firewall → Allow an app**

Disabling the firewall is acceptable for a controlled lab/demo on a trusted LAN but is **not recommended** for everyday use.

---

## 10. Key Socket Functions Reference

| Function        | Used By  | Description                                      |
|-----------------|----------|--------------------------------------------------|
| `WSAStartup`    | Both     | Initialize Winsock DLL                           |
| `socket`        | Both     | Create a socket endpoint                         |
| `bind`          | Server   | Attach socket to local IP/port                   |
| `listen`        | Server   | Mark socket as passive (accepting connections)   |
| `accept`        | Server   | Accept incoming TCP connection                   |
| `connect`       | Client   | Initiate TCP connection to remote host           |
| `send`          | Both     | Send bytes over connected socket                 |
| `recv`          | Both     | Receive bytes from connected socket              |
| `closesocket`   | Both     | Close socket and free resources                    |
| `WSACleanup`    | Both     | Shut down Winsock DLL                            |
| `htons`         | Both     | Host-to-network short (port byte order)          |
| `inet_pton`     | Client   | String IP → binary address                       |

---

## 11. Program Limitations

Understanding these limitations is important for a complete project analysis:

1. **Single client only** — After one `accept()`, no other clients can connect until the server restarts.
2. **Hardcoded IP** — The client must be recompiled if the server IP changes (unless changed to prompt for IP at runtime).
3. **No message framing** — Large messages or rapid sends may be split across multiple `recv()` calls; there is no protocol to reassemble them.
4. **Plain text only** — No encryption (TLS/SSL); traffic is visible on the network.
5. **No error recovery** — Most failures cause immediate exit with a short error message.
6. **Global socket variables** — Used so the receiver thread can access the connection socket; works for this simple design but is not ideal for larger programs.
7. **Blocking I/O** — `recv()` and `getline()` block; the program cannot easily handle timeouts or multiple clients.

---

## 12. Summary

This project demonstrates fundamental **Computer Networks** concepts:

- **Client–server architecture**
- **TCP connection establishment** (three-way handshake via `connect` / `accept`)
- **Socket programming** with the Windows Winsock2 API
- **Port binding and listening** on the server side
- **Concurrent bidirectional communication** using C++ threads
- **Real-world networking issues** such as firewall rules and LAN IP addressing

The server listens on all interfaces at port 8080, accepts one client, and exchanges text messages in real time. The client connects to the server’s LAN IP (192.168.0.107), and both programs use a sender thread (main) and receiver thread (background) to achieve interactive chat. Compilation via MSYS2 produces Windows executables run from CMD; firewall configuration on the server was required for successful connection establishment.

---

## 13. File Overview

| File                 | Description                                      |
|----------------------|--------------------------------------------------|
| `server.cpp`         | TCP server source code                           |
| `client.cpp`         | TCP client source code                           |
| `New Text Document.txt` | Server machine IPv4 address (192.168.0.107) |
| `REPORT.md`          | This project report                              |
