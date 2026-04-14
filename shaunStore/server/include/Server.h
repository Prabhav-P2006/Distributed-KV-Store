#ifndef SERVER_HPP
#define SERVER_HPP

#include <atomic>
#include <unordered_map>
#include <string>

class Server {
    private:
        int port;
        int serverSocket;
        std::atomic<bool> running;
        const unsigned int MAX_CLIENTS = 32; // Maximum number of clients to handle
        const int BUFFER_SIZE = 4096; // Size of the buffer for reading data
        
        struct Client {
            int socket;
            std::string readBuffer;
            std::string writeBuffer;
            bool hasPendingWrite = false;
        };

        std::unordered_map<int, Client> clients;

    public:
        Server(int port);
        ~Server() = default;  
        
        void shutdown();
        void run();

        void setupSignalHandler();
};

#endif