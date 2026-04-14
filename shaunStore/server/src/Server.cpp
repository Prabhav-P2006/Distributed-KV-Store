#include "../include/Server.h"
#include "../include/CommandHandler.h"
#include "../include/Database.h"
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <vector>
#include <cstring>
#include <sys/epoll.h> 
#include <fcntl.h>
#include <signal.h>

static Server* server = nullptr;

void signalHandler(int signum) {
    if (server) {
        std::cout << "Received signal " << signum << ". Shutting down server." << std::endl;
        server->shutdown();
    }
    exit(signum);
}

void Server::setupSignalHandler() {
    signal(SIGINT, signalHandler); // Handles Ctrl+C (Keyboard interrupt)
}

Server::Server(int port) : port(port), serverSocket(-1), running(true) {
    server = this;
    setupSignalHandler();
}

void Server::shutdown() {
    running = false;
    std::cout << "Server shutdown complete." << std::endl;
}

void Server::run() {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Failed to create socket." << std::endl;
        return;
    }

    // Set the server socket to non-blocking mode
    if (fcntl(serverSocket, F_SETFL, O_NONBLOCK) < 0) {
        std::cerr << "Failed to set server socket to non-blocking mode." << std::endl;
        close(serverSocket);
        serverSocket = -1;
        return;
    }

    int val = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
        std::cerr << "Failed to set socket options." << std::endl;
        close(serverSocket);
        serverSocket = -1;
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr*) &serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to bind socket." << std::endl;
        close(serverSocket);
        serverSocket = -1;
        return;
    }

    if (listen(serverSocket, SOMAXCONN) < 0) {
        std::cerr << "Failed to listen on socket." << std::endl;
        close(serverSocket);
        serverSocket = -1;
        return;
    }

    std::cout << "Server is running on port " << port << std::endl;

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = serverSocket;

    int epoll_fd = epoll_create1(0);

    if (epoll_fd < 0) {
        std::cerr << "Failed to create epoll instance." << std::endl;
        close(serverSocket);
        return;
    }

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serverSocket, &ev) == -1) {
        std::cerr << "Failed to add server socket to epoll." << std::endl;
        close(epoll_fd);
        return;
    }

    CommandHandler commandHandler;

    // Create an array to hold events
    struct epoll_event events[MAX_CLIENTS];

    while (running) {
        int n = epoll_wait(epoll_fd, events, MAX_CLIENTS, -1);

        for (int i =0; i < n; i++) {
            int fd = events[i].data.fd;
            // Accept a new client connection
            if (fd == serverSocket) {
                struct sockaddr_in clientAddr;
                socklen_t clientAddrLen = sizeof(clientAddr);

                int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
                if (clientSocket < 0) {
                    std::cerr << "Failed to accept client connection." << std::endl;
                    continue;
                }
                else {
                    if (clients.size() >= MAX_CLIENTS) {
                        std::cerr << "Maximum number of clients reached. Closing new connection." << std::endl;
                        close(clientSocket);
                        continue;
                    }
                    std::clog << "Accepted new client connection: " << clientSocket << std::endl;
                    // Set client socket to non-blocking mode
                    if (fcntl(clientSocket, F_SETFL, O_NONBLOCK) < 0) {
                        std::cerr << "Failed to set client socket to non-blocking mode." << std::endl;
                        close(clientSocket);
                        continue;
                    }
                    struct epoll_event clientEv;
                    clientEv.events = EPOLLIN | EPOLLET;
                    clientEv.data.fd = clientSocket;

                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, clientSocket, &clientEv) == -1) {
                        std::cerr << "Failed to add client socket to epoll." << std::endl;
                        close(clientSocket);
                        std::clog << "Client connection closed: " << clientSocket << std::endl;
                        continue;
                    }

                    clients[clientSocket] = Client{clientSocket, "", "", false};
                }
            }
            else if (events[i].events & (EPOLLIN | EPOLLOUT)) {
                int clientFd = fd;
                auto& client = clients[clientFd];

                if (events[i].events & EPOLLIN) {
                    char buffer[BUFFER_SIZE];
                    
                    while (true) {
                        ssize_t bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
                        if (bytesRead < 0) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                // No more data to read
                                break;
                            }
                            std::cerr << "Error reading from client socket." << std::endl;
                            close(clientFd);
                            std::clog << "Client connection closed: " << clientFd << std::endl;
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, clientFd, nullptr);
                            clients.erase(clientFd);
                            break;
                        } else if (bytesRead == 0) {
                            // Client disconnected
                            close(clientFd);
                            std::clog << "Client connection closed: " << clientFd << std::endl;
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, clientFd, nullptr);
                            clients.erase(clientFd);
                            break;
                        }
                        client.readBuffer.append(buffer, bytesRead);

                        while (true) {
                            std::vector<std::string> parsedCommand;
                            size_t parsedLen = 0;

                            if (!commandHandler.parseRESP(client.readBuffer, parsedCommand, parsedLen)) {
                                parsedLen = 0; // Reset parsed length if parsing fails
                                break; // Not enough data to parse a complete command
                            }
                            // Handle the command
                            std::string response = commandHandler.handleCommand(parsedCommand);
                            std::cout << "Response: " << response << std::endl;
                            client.writeBuffer.append(response);
                            client.hasPendingWrite = true; // Set pending write flag

                            struct epoll_event writeEv;
                            writeEv.events = EPOLLIN | EPOLLOUT | EPOLLET;
                            writeEv.data.fd = clientFd;
                            if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, clientFd, &writeEv) == -1) {
                                std::cerr << "Failed to modify client socket for write." << std::endl;
                                close(clientFd);
                                std::clog << "Client connection closed: " << clientFd << std::endl;
                                clients.erase(clientFd);
                                break;
                            }

                            // Remove the processed part from the read buffer
                            client.readBuffer.erase(0, parsedLen);
                        }
                    }
                }
                
                if (events[i].events & EPOLLOUT) {
                    while (client.hasPendingWrite && !client.writeBuffer.empty()) {
                        ssize_t bytesWritten = send(clientFd, client.writeBuffer.c_str(), client.writeBuffer.size(), 0);
                        if (bytesWritten < 0) {
                            std::cerr << "Error writing to client socket." << std::endl;
                            close(clientFd);
                            std::clog << "Client connection closed: " << clientFd << std::endl;
                            clients.erase(clientFd);
                            break;
                        }
                        client.writeBuffer.erase(0, bytesWritten);
                        if (client.writeBuffer.empty()) {
                            client.hasPendingWrite = false;
                            
                            struct epoll_event readEv;
                            readEv.events = EPOLLIN | EPOLLET;
                            readEv.data.fd = clientFd;
                            if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, clientFd, &readEv) == -1) {
                                std::cerr << "Failed to modify client socket for read." << std::endl;
                                close(clientFd);
                                std::clog << "Client connection closed: " << clientFd << std::endl;
                                clients.erase(clientFd);
                                break;
                            }
                        }
                    }
                }
                
            }
            
        }
    }

    for (auto & client : clients) {
        close(client.second.socket);
    }

    close(epoll_fd);
    close(serverSocket);

    if (Database::getInstance().dumpDatabase("dump")) {
        std::cout << "Database dumped successfully." << std::endl;
    } else {
        std::cerr << "Failed to dump database." << std::endl;
    }


}