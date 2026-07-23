// mock_server.cpp
// Minimal HTTP server for load balancer testing.
// Usage: ./mock_backend <port> <backend_id>
// Example: ./mock_backend 9001 backend1

#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <backend_id>\n";
        return 1;
    }

    int port = std::stoi(argv[1]);
    std::string backend_id = argv[2];

    // 1. Create a TCP socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        return 1;
    }

    // Allow immediate reuse of the port after restart (avoids "address in use" errors)
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Bind it to the given port on all interfaces
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return 1;
    }

    // 3. Start listening for incoming connections (queue up to 10 pending)
    if (listen(server_fd, 10) < 0) {
        perror("listen failed");
        return 1;
    }

    std::cout << backend_id << " listening on port " << port << "\n";

    // 4. Accept loop — handle one connection at a time (fine for now, no threading yet)
    while (true) {
        sockaddr_in client_address{};
        socklen_t client_len = sizeof(client_address);
        int client_fd = accept(server_fd, (sockaddr*)&client_address, &client_len);
        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }

        // Read the incoming request (we don't fully parse it yet — just enough
        // to see the request line, e.g. "GET /health HTTP/1.1")
        char buffer[4096] = {0};
        read(client_fd, buffer, sizeof(buffer) - 1);
        std::string request(buffer);

        // Figure out which path was requested
        std::string response_body;
        std::string status_line;

        if (request.find("GET /health") == 0 || request.find("GET /health ") != std::string::npos) {
            status_line = "HTTP/1.1 200 OK\r\n";
            response_body = backend_id + " healthy\n";
        } else {
            status_line = "HTTP/1.1 200 OK\r\n";
            response_body = "Hello from " + backend_id + "\n";
        }

        std::string response =
            status_line +
            "Content-Type: text/plain\r\n" +
            "Content-Length: " + std::to_string(response_body.size()) + "\r\n" +
            "Connection: close\r\n" +
            "\r\n" +
            response_body;

        write(client_fd, response.c_str(), response.size());
        close(client_fd);
    }

    close(server_fd);
    return 0;
}