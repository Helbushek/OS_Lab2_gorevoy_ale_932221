#include <iostream>
#include <thread>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <chrono>
#include <cstring>

#include <signal.h>
#include <csignal>

int serverSocket = -1;
int clientSocket = -1;
constexpr int PortNumber = 8009;

bool isFinished = false;
bool isConnected = false;

volatile sig_atomic_t wasSigHup = 0;

sigset_t origMask;
sigset_t blockedMask;

void Server()
{
    while (!isFinished)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
    
        if (serverSocket == -1)
        {
            break;
        }

        FD_SET(serverSocket, &readfds);
        int maxFd = serverSocket;

        if (isConnected && clientSocket != -1)
        {
            FD_SET(clientSocket, &readfds);
            if (clientSocket > maxFd) maxFd = clientSocket;
        }

        int ready = pselect(maxFd + 1, &readfds, nullptr, nullptr, nullptr, &origMask);

        if (ready == -1)
        {
            if (errno == EINTR)
            {
                if (wasSigHup)
                {
                    std::cout << "Received SIGHUP" << std::endl;
                    isFinished = true;
                    wasSigHup = 0;
                }
                continue;
            }

            std::cerr << strerror(errno) << std::endl;
            break;
        }

        if (FD_ISSET(serverSocket, &readfds) && !isConnected)
        {
            sockaddr_in cliaddr{};
            socklen_t len = sizeof(cliaddr);
            int fd = accept(serverSocket, (sockaddr*)&cliaddr, &len);

            if (fd == -1)
            {
                std::cerr << "Error occurred: " << strerror(errno) << std::endl;
                continue;
            }

            clientSocket = fd;
            isConnected = true;
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &cliaddr.sin_addr, ip, sizeof(ip));
            std::cout << "Client connected: " << ip << ":" << ntohs(cliaddr.sin_port) << std::endl;
        }

        if (isConnected && FD_ISSET(clientSocket, &readfds))
        {
            char buf[1024];
            ssize_t n = recv(clientSocket, buf, sizeof(buf), 0);

            if (n == 0)
            {
                std::cout << "Client disconnected" << std::endl;
                close(clientSocket);
                clientSocket = -1;
                isConnected = false;
            }
            else if (n < 0)
            {
                std::cerr << "Error occurred: " << strerror(errno) << std::endl;
                close(clientSocket);
                clientSocket = -1;
                isConnected = false;
            }
            else
            {
                std::cout << "Received " << std::to_string(n) << " bytes: " << std::string(buf, n) << std::endl;
            }
        }
    }
}

bool InitializeServer()
{
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        std::cerr << "Couldn`t open server`s socket" << std::endl; 
        return false;
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY; 
    serverAddress.sin_port = htons(PortNumber);
    
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) 
    {
        std::cerr << "Couldn`t bind socket to address" << std::endl;
        std::cerr << strerror(errno) << std::endl;
        return false;
    }

    if (listen(serverSocket, SOMAXCONN) == -1) 
    {
        std::cerr << "Error during socket listen" << std::endl;
        return false;
    }

    return true;
}

extern "C" void SignalHandler(int)
{
    wasSigHup = 1; 
}

int main()
{
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);

    if (sigprocmask(SIG_BLOCK, &blockedMask, &origMask) == -1)
    {
        std::cout << "Sigprocmask failed: " << strerror(errno) << std::endl;
        return EXIT_FAILURE;
    }

    struct sigaction sa{};
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = SignalHandler;
    sa.sa_flags = 0;

    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        std::cerr << "Error registering SIGINT handler" << std::endl;
        return EXIT_FAILURE;
    }

    if (!InitializeServer())
    {
        return EXIT_FAILURE;
    }

    std::thread serverThread(Server);
    serverThread.join();
    close(serverSocket);
    if (clientSocket != -1)
    {
        close(clientSocket);
    }

    return EXIT_SUCCESS;
}