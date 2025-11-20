#include <iostream>
#include <thread>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <chrono>
#include <cstring>

#include <signal.h>
#include <csignal>

int serverSocket;
int clientSocket;
constexpr int PortNumber = 8009;

bool isFinished = false;
bool isConnected = false;

volatile sig_atomic_t wasSigHup = 0;

void Server()
{
    while (!isFinished)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
    
        FD_SET(serverSocket, &readfds);
        int maxFd = serverSocket;

        FD_SET(clientSocket, &readfds);
        if (clientSocket > maxFd)
        {
            maxFd = clientSocket;
        }

        int ready = select(maxFd + 1, &readfds, nullptr, nullptr, nullptr);

        if (ready == -1)
        {
            std::cerr << strerror(errno) << std::endl;
            break;
        }

        if (FD_ISSET(serverSocket, &readfds) && !isConnected)
        {
            sockaddr_in cliaddr{};
            socklen_t len = sizeof(cliaddr);
            clientSocket = accept(serverSocket, (sockaddr*)&cliaddr, &len);

            if (clientSocket >= 0)
            {
                std::cout << "Connected: " << std::to_string(clientSocket) << std::endl;
            }
            isConnected = true;
        }

        if (!isConnected)
        {
            continue;
        }

        int fd = clientSocket;
        if (FD_ISSET(fd, &readfds))
        {
            char buf[1024];
            ssize_t n = recv(fd, buf, sizeof(buf), 0);

            if (n <= 0)
            {
                std::cout << "Client disconnected" << std::endl;
                isConnected = false;
            }
            else
            {
                std::cout << "Received " << std::to_string(n) << " bytes: " << std::string(buf) << std::endl;
            }
        }
    }
}

void InitializeServer()
{
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        std::cout << "Couldn`t open server`s socket" << std::endl; 
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY; 
    serverAddress.sin_port = htons(PortNumber);

    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) 
    {
        std::cout << "Couldn`t bind socket to address" << std::endl;
        std::cout << strerror(errno) << std::endl;
        return;
    }

    if (listen(serverSocket, SOMAXCONN) == -1) 
    {
        std::cout << "Error during socket listen" << std::endl;
    }
}

extern "C" void SignalHandler(int signum)
{
    wasSigHup = signum;
    std::cout << "Caught signal " << signum << ". Exiting gracefully..." << std::endl;
    // Perform necessary cleanup here
    exit(signum);
}

int main()
{
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = SignalHandler;

    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        std::cerr << "Error registering SIGINT handler" << std::endl;
        return EXIT_FAILURE;
    }

    InitializeServer();

    std::thread serverThread(Server);
    serverThread.join();

    return EXIT_SUCCESS;
}